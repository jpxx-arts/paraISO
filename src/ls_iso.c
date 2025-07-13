#include "iso9660.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo.iso> <caminho_do_diretorio>\n", argv[0]);
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

    printf("--- Procurando pelo diretorio '%s' ---\n\n", argv[2]);
    DirectoryEntry *dir_to_list = find_directory_entry(iso_file, &pvd, argv[2]);

    if (dir_to_list) {
        list_directory_contents(iso_file, dir_to_list);
        free(dir_to_list);
    } else {
        fprintf(stderr, "Erro: Nao foi possivel encontrar o diretorio '%s'.\n", argv[2]);
    }

    fclose(iso_file);
    return 0;
}
