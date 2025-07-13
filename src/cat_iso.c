#include "iso9660.h"
#include <stdlib.h>

void cat_file(FILE *iso_file, const DirectoryEntry *file_entry) {
    uint32_t size = file_entry->data_length_le;
    if (size == 0) return;

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
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo.iso> <caminho_do_arquivo>\n", argv[0]);
        return 1;
    }

    FILE *iso_file = fopen(argv[1], "rb");
    if (!iso_file) {
        perror("Erro ao abrir o arquivo ISO");
        return 1;
    }
    
    PrimaryVolumeDescriptor pvd;
    if (!find_pvd(iso_file, &pvd)) {
        fclose(iso_file);
        return 1;
    }

    printf("--- Procurando por '%s' ---\n\n", argv[2]);
    DirectoryEntry *file_to_cat = find_directory_entry(iso_file, &pvd, argv[2]);

    if (file_to_cat) {
        if (file_to_cat->file_flags & 0x02) {
             fprintf(stderr, "Erro: '%s' e um diretorio, nao um arquivo.\n", argv[2]);
        } else {
            cat_file(iso_file, file_to_cat);
        }
        free(file_to_cat);
    } else {
        fprintf(stderr, "\n--- Arquivo '%s' nao encontrado. ---\n", argv[2]);
    }
    
    fclose(iso_file);
    return 0;
}
