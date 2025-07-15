#include "iso9660.h"

// Retorna o número do diretório correspondente ao offset de uma entrada na Path Table
uint16_t offset_to_dir_number(uint8_t *path_table, uint32_t table_size, uint32_t target_offset) {
    uint32_t offset = 0;
    uint16_t index = 1;

    while (offset < table_size) {
        if (offset == target_offset) return index;

        PathTableRecord *entry = (PathTableRecord *)(path_table + offset);
        uint16_t size = 8 + entry->identifier_len + (entry->identifier_len % 2 != 0);
        offset += size;
        index++;
    }

    return 0; // erro
}

// Função auxiliar para verificar extensão
static bool has_extension(const char *name, const char *extension) {
    if (!extension) return true;
    const char *point = strrchr(name, '.');
    return point && strcasecmp(point, extension) == 0;
}

bool find_pvd(FILE *iso_file, PrimaryVolumeDescriptor *pvd) {
    fseek(iso_file, 16 * LOGICAL_SECTOR_SIZE, SEEK_SET);
    if (fread(pvd, sizeof(PrimaryVolumeDescriptor), 1, iso_file) != 1) {
        perror("Erro: Falha ao ler o PVD do arquivo ISO");
        return false;
    }

    if (pvd->type_code != 1 || strncmp(pvd->standard_identifier, "CD001", 5) != 0) {
        fprintf(stderr, "Erro: Assinatura 'CD001' nao encontrada. A imagem ISO pode estar corrompida ou invalida.\n");
        return false;
    }
    return true;
}

// Divide o caminho em componentes separados por '/' (ex: "/boot/isolinux/f2" → ["boot", "isolinux", "f2"])
int split_path(const char *path, char *components[], int max_depth) {
    static char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    if (path_copy[0] == '/')
        memmove(path_copy, path_copy + 1, strlen(path_copy));

    int depth = 0;
    char *token = strtok(path_copy, "/");
    while (token && depth < max_depth) {
        components[depth++] = token;
        token = strtok(NULL, "/");
    }
    return depth;
}

// Lê a Path Table da imagem ISO para a memória
uint8_t* read_path_table(FILE *iso_file, const PrimaryVolumeDescriptor *pvd) {
    uint8_t *buf = malloc(pvd->path_table_size_le);
    if (!buf) return NULL;

    fseek(iso_file, pvd->loc_path_table_le * LOGICAL_SECTOR_SIZE, SEEK_SET);
    if (fread(buf, pvd->path_table_size_le, 1, iso_file) != 1) {
        free(buf);
        return NULL;
    }

    return buf;
}

// Caminha pela Path Table para encontrar o LBA do diretório pai de um caminho
int resolve_parent_directory(uint8_t *path_table, uint32_t table_size, char **components, int count, uint32_t *extent_out) {
    uint16_t parent_number = 1;
    PathTableRecord *match = NULL;

    for (int level = 0; level < count; level++) {
        uint32_t offset = 0;
        match = NULL;

        while (offset < table_size) {
            PathTableRecord *entry = (PathTableRecord *)(path_table + offset);
            if (entry->parent_dir_number == parent_number &&
                entry->identifier_len == strlen(components[level]) &&
                strncasecmp(entry->directory_identifier, components[level], entry->identifier_len) == 0) {
                match = entry;
                parent_number = offset_to_dir_number(path_table, table_size, offset);
                break;
            }
            uint16_t size = 8 + entry->identifier_len + (entry->identifier_len % 2 != 0);
            offset += size;
        }

        if (!match) return 0;
    }

    *extent_out = match->extent_location;
    return 1;
}

// Varre um diretório buscando um arquivo ou subdiretório com o nome especificado (ignorando sufixo ';1')
DirectoryEntry* find_in_directory(FILE *iso_file, uint32_t extent_lba, const char *target) {
    DirectoryEntry *result = NULL;
    uint32_t size;

    {
        DirectoryEntry temp;
        fseek(iso_file, extent_lba * LOGICAL_SECTOR_SIZE, SEEK_SET);
        fread(&temp, sizeof(DirectoryEntry), 1, iso_file);
        size = temp.data_length_le;
    }

    uint8_t *buffer = malloc(size);
    if (!buffer) return NULL;

    fseek(iso_file, extent_lba * LOGICAL_SECTOR_SIZE, SEEK_SET);
    fread(buffer, 1, size, iso_file);

    uint32_t offset = 0;
    while (offset < size) {
        DirectoryEntry *entry = (DirectoryEntry *)(buffer + offset);
        if (entry->length_of_record == 0) break;

        char entry_name[256] = {0};
        int len = 0;
        for (int i = 0; i < entry->file_identifier_len && len < 255; ++i) {
            if (entry->file_identifier[i] == ';') break;
            entry_name[len++] = entry->file_identifier[i];
        }
        entry_name[len] = '\0';

        if (strcasecmp(entry_name, target) == 0) {
            result = malloc(entry->length_of_record);
            if (result)
                memcpy(result, entry, entry->length_of_record);
            break;
        }

        offset += entry->length_of_record;
    }

    free(buffer);
    return result;
}

DirectoryEntry* find_directory_entry(FILE *iso_file, const PrimaryVolumeDescriptor *pvd, const char *path) {
    if (strcmp(path, "/") == 0) {
        DirectoryEntry *root = malloc(sizeof(DirectoryEntry));
        if (!root) return NULL;
        memcpy(root, &pvd->root_directory_entry, sizeof(DirectoryEntry));
        return root;
    }

    char *components[64];
    int depth = split_path(path, components, 64);
    if (depth == 0) return NULL;

    uint8_t *path_table = read_path_table(iso_file, pvd);
    if (!path_table) return NULL;

    uint32_t parent_extent;
    if (!resolve_parent_directory(path_table, pvd->path_table_size_le, components, depth - 1, &parent_extent)) {
        free(path_table);
        return NULL;
    }

    free(path_table);
    return find_in_directory(iso_file, parent_extent, components[depth - 1]);
}

void list_directory_contents(FILE *iso_file, const DirectoryEntry *dir_entry, const char *extension) {
    if (!(dir_entry->file_flags & 0x02)) {
        fprintf(stderr, "Erro: A entrada fornecida nao e um diretorio.\n");
        return;
    }

    uint32_t dir_lba = dir_entry->extent_location_le;
    uint32_t dir_size = dir_entry->data_length_le;
    uint8_t *dir_buffer = malloc(dir_size);
    if (!dir_buffer) {
        perror("Erro: Falha ao alocar memoria para o buffer do diretorio");
        return;
    }

    fseek(iso_file, dir_lba * LOGICAL_SECTOR_SIZE, SEEK_SET);
    fread(dir_buffer, dir_size, 1, iso_file);
    
    printf("Permissoes  Tamanho      Nome\n");
    printf("----------  -----------  ----------------\n");

    uint32_t offset = 0;
    while (offset < dir_size) {
        DirectoryEntry *entry = (DirectoryEntry *)(dir_buffer + offset);
        if (entry->length_of_record == 0) break;

        if (entry->file_identifier_len == 1 && (entry->file_identifier[0] == 0 || entry->file_identifier[0] == 1)) {
            offset += entry->length_of_record;
            continue;
        }

        char name[256] = {0};
        int len = 0;
        for (int i = 0; i < entry->file_identifier_len; ++i) {
            if (entry->file_identifier[i] == ';') break; // Ignora a versão ";1"
            name[len++] = entry->file_identifier[i];
        }
        name[len] = '\0';

        char type = (entry->file_flags & 0x02) ? 'd' : '-';

        if (type == 'd' || has_extension(name, extension)) {
            printf("%c--------- %11u  ", type, entry->data_length_le);
            
            printf("%s", name);
            
            if (type == 'd') putchar('/');
            putchar('\n');
        }
        
        offset += entry->length_of_record;
    }

    free(dir_buffer);
}

// Extrai recursivamente os arquivos a partir de um diretório
void extract_directory(FILE *iso_file, const DirectoryEntry *dir_entry, const char *extension) {
    if (!(dir_entry->file_flags & 0x02)) {
        fprintf(stderr, "Erro: A entrada fornecida nao e um diretorio.\n");
        return;
    }

    uint32_t size = dir_entry->data_length_le;
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        perror("Erro: Falha ao alocar memoria para leitura do diretorio");
        return;
    }

    fseek(iso_file, dir_entry->extent_location_le * LOGICAL_SECTOR_SIZE, SEEK_SET);
    fread(buffer, size, 1, iso_file);

    uint32_t offset = 0;
    while (offset < size) {
        DirectoryEntry *entry = (DirectoryEntry *)(buffer + offset);
        if (entry->length_of_record == 0) break;

        if (entry->file_identifier_len == 1 &&
            (entry->file_identifier[0] == 0 || entry->file_identifier[0] == 1)) {
            offset += entry->length_of_record;
            continue;
        }

        char name[256] = {0};
        int len = 0;
        for (int i = 0; i < entry->file_identifier_len; ++i) {
            if (entry->file_identifier[i] == ';') break;
            name[len++] = entry->file_identifier[i];
        }
        name[len] = '\0';

        // Cria diretório, se necessário
        if (entry->file_flags & 0x02) {
            mkdir(name, 0755);
            DirectoryEntry *subdir = malloc(entry->length_of_record);
            if (subdir) {
                memcpy(subdir, entry, entry->length_of_record);
                if (chdir(name) == 0) {
                    extract_directory(iso_file, subdir, extension);
                    chdir("..");
                } else {
                    fprintf(stderr, "Erro: Nao foi possivel entrar no diretorio '%s'.\n", name);
                }
                free(subdir);
            }
        } else if (has_extension(name, extension)) {
            FILE *out = fopen(name, "wb");
            if (!out) {
                fprintf(stderr, "Erro: Nao foi possivel criar o arquivo '%s'.\n", name);
                perror("Detalhes");
                offset += entry->length_of_record;
                continue;
            }

            uint32_t file_size = entry->data_length_le;
            uint8_t *data = malloc(file_size);
            if (!data) {
                perror("Erro ao alocar memoria para extracao");
                fclose(out);
                offset += entry->length_of_record;
                continue;
            }

            fseek(iso_file, entry->extent_location_le * LOGICAL_SECTOR_SIZE, SEEK_SET);
            fread(data, 1, file_size, iso_file);
            fwrite(data, 1, file_size, out);

            fclose(out);
            free(data);

            printf("Arquivo extraido: %s (%u bytes)\n", name, file_size);
        }

        offset += entry->length_of_record;
    }

    free(buffer);
}


void cat_file(FILE *iso_file, const DirectoryEntry *file_entry) {
    uint32_t size = file_entry->data_length_le;
    if (size == 0){
        fprintf(stderr, "Aviso: O arquivo esta vazio.\n");
        return;
    }

    fseek(iso_file, file_entry->extent_location_le * LOGICAL_SECTOR_SIZE, SEEK_SET);

    char buffer[4096];
    size_t bytes_to_read, bytes_read;
    
    while (size > 0) {
        bytes_to_read = (size > sizeof(buffer)) ? sizeof(buffer) : size;
        bytes_read = fread(buffer, 1, bytes_to_read, iso_file);
        if (bytes_read > 0) {
            fwrite(buffer, 1, bytes_read, stdout);
            size -= bytes_read;
        } else {
            fprintf(stderr, "\nErro: Falha ao ler os dados do arquivo.\n");
            break;
        }
    }
}


