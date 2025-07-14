#include "iso9660.h"

// Função auxiliar para verificar extensão
static bool has_extension(const char *name, const char *extension) {
    if (!extension) return true;
    const char *point = strrchr(name, '.');
    return point && strcasecmp(point, extension) == 0;
}

// Função para comparar nomes de arquivo ISO 9660 (case-insensitive e ignorando a versão ';1' (padrão de sufixo para versão em sistemas antigos))
static bool compare_iso_identifier(const char* token, const char* iso_name, uint8_t iso_name_len) {
    size_t token_len = strlen(token);
    
    if (token_len == iso_name_len && strncasecmp(token, iso_name, token_len) == 0) {
        return true;
    }
    
    // Comparação ignorando a versão do arquivo (ex: "README.TXT;1")
    if (iso_name_len > 2 && iso_name[iso_name_len - 2] == ';') {
        if (token_len == iso_name_len - 2 && strncasecmp(token, iso_name, token_len) == 0) {
            return true;
        }
    }
    return false;
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

DirectoryEntry* find_directory_entry(FILE *iso_file, const PrimaryVolumeDescriptor *pvd, const char *path) {
    if (strcmp(path, "/") == 0) {
        DirectoryEntry *root_entry = malloc(sizeof(DirectoryEntry));
        if (!root_entry) return NULL;
        memcpy(root_entry, &pvd->root_directory_entry, sizeof(DirectoryEntry));
        return root_entry;
    }
    
    // 1. Carregar a Path Table para a memória
    uint8_t *path_table_buffer = malloc(pvd->path_table_size_le);
    if (!path_table_buffer) {
        perror("Erro: Falha ao alocar memoria para a Path Table");
        return NULL;
    }
    fseek(iso_file, pvd->loc_path_table_le * LOGICAL_SECTOR_SIZE, SEEK_SET);
    if (fread(path_table_buffer, pvd->path_table_size_le, 1, iso_file) != 1) {
        perror("Erro: Falha ao ler a Path Table");
        free(path_table_buffer);
        return NULL;
    }

    // 2. Navegar na Path Table para encontrar o diretório PAI do alvo final
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *last_component = strrchr(path_copy, '/');
    if (!last_component) {
        free(path_table_buffer);
        return NULL;
    }
    *last_component = '\0';
    last_component++;

    char *parent_path = path_copy;
    if (strlen(parent_path) == 0) parent_path = "/";

    // Encontra a entrada do diretório pai
    DirectoryEntry *parent_dir_entry = find_directory_entry(iso_file, pvd, parent_path);
    free(path_table_buffer);

    if (!parent_dir_entry) {
        fprintf(stderr, "Erro: Diretorio pai '%s' nao encontrado.\n", parent_path);
        return NULL;
    }
    
    // 3. Varredura linear dentro do diretório pai para encontrar o alvo final
    uint32_t dir_size = parent_dir_entry->data_length_le;
    uint8_t *dir_buffer = malloc(dir_size);
    if (!dir_buffer) {
        fprintf(stderr, "Erro: Falha ao alocar memoria para leitura do diretorio.\n");
        free(parent_dir_entry);
        return NULL;
    }

    fseek(iso_file, parent_dir_entry->extent_location_le * LOGICAL_SECTOR_SIZE, SEEK_SET);
    fread(dir_buffer, dir_size, 1, iso_file);
    free(parent_dir_entry);

    DirectoryEntry *found_entry = NULL;
    uint32_t offset = 0;
    while(offset < dir_size) {
        DirectoryEntry *entry = (DirectoryEntry *)(dir_buffer + offset);
        if (entry->length_of_record == 0) break;

        if (compare_iso_identifier(last_component, entry->file_identifier, entry->file_identifier_len)) {
            found_entry = malloc(entry->length_of_record);
            if (found_entry) {
                memcpy(found_entry, entry, entry->length_of_record);
            } else{
                fprintf(stderr, "Erro: Falha ao alocar memoria para a entrada de diretorio encontrada.\n");
            }
            break;
        }
        offset += entry->length_of_record;
    }
    
    free(dir_buffer);
    return found_entry;
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


