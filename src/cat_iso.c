#include "iso9660.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo.iso> <caminho_do_arquivo>\n", argv[0]);
        return 1;
    }

    FILE *iso_file = fopen(argv[1], "rb");
    if (!iso_file) {
        fprintf(stderr, "Erro: Nao foi possivel abrir o arquivo ISO '%s'.\n", argv[1]);
        perror("Detalhes");
        return 1;
    }
    
    PrimaryVolumeDescriptor pvd;
    if (!find_pvd(iso_file, &pvd)) {
        fprintf(stderr, "Erro: Descritor de volume primario nao encontrado no ISO.\n");
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
