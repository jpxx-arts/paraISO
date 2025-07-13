#include "iso9660.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
        perror("Erro ao ler o PVD do arquivo ISO");
        return false;
    }

    if (pvd->type_code != 1 || strncmp(pvd->standard_identifier, "CD001", 5) != 0) {
        fprintf(stderr, "Erro: Assinatura 'CD001' nao encontrada. Nao e uma ISO valida.\n");
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
        perror("Falha ao alocar memoria para a Path Table");
        return NULL;
    }
    fseek(iso_file, pvd->loc_path_table_le * LOGICAL_SECTOR_SIZE, SEEK_SET);
    if (fread(path_table_buffer, pvd->path_table_size_le, 1, iso_file) != 1) {
        perror("Falha ao ler a Path Table");
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
        return NULL;
    }
    
    // 3. Varredura linear dentro do diretório pai para encontrar o alvo final
    uint32_t dir_size = parent_dir_entry->data_length_le;
    uint8_t *dir_buffer = malloc(dir_size);
    if (!dir_buffer) {
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
            }
            break;
        }
        offset += entry->length_of_record;
    }
    
    free(dir_buffer);
    return found_entry;
}

void list_directory_contents(FILE *iso_file, const DirectoryEntry *dir_entry) {
    if (!(dir_entry->file_flags & 0x02)) {
        fprintf(stderr, "Erro: A entrada fornecida nao e um diretorio.\n");
        return;
    }

    uint32_t dir_lba = dir_entry->extent_location_le;
    uint32_t dir_size = dir_entry->data_length_le;
    uint8_t *dir_buffer = malloc(dir_size);
    if (!dir_buffer) {
        perror("Falha ao alocar memoria para o buffer do diretorio");
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

        char type = (entry->file_flags & 0x02) ? 'd' : '-';
        printf("%c--------- %11u  ", type, entry->data_length_le);

        for (int i = 0; i < entry->file_identifier_len; ++i) {
            // Ignora a versão do arquivo ";1"
            if (entry->file_identifier[i] == ';') break;
            putchar(entry->file_identifier[i]);
        }
        
        if (type == 'd') putchar('/');
        putchar('\n');
        
        offset += entry->length_of_record;
    }

    free(dir_buffer);
}
