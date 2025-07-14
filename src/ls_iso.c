#include "iso9660.h"

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Uso: %s [list|extract] <file.iso> <path> [extension]\n", argv[0]);
        return 1;
    }

    const char *mode = "list";
    const char *file_iso = NULL;
    const char *path = NULL;
    const char *filter_ext = NULL;
    int arg_offset = 1;

    if (argc > 3 && (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "extract") == 0)) {
        mode = argv[1];
        arg_offset = 2;
    }

    if (argc - arg_offset < 2) {
        fprintf(stderr, "Erro: Argumentos insuficientes para <file.iso> e <path>.\n");
        fprintf(stderr, "Uso: %s [list|extract] <file.iso> <path> [extension]\n", argv[0]);
        return 1;
    }

    file_iso = argv[arg_offset];
    path = argv[arg_offset + 1];

    if (argc - arg_offset == 3) {
        filter_ext = argv[arg_offset + 2];
    }

    FILE *iso_file = fopen(file_iso, "rb");
    if (!iso_file) {
        fprintf(stderr, "Erro: Nao foi possivel abrir o arquivo ISO '%s'.\n", file_iso);
        perror("Detalhes");
        return 1;
    }

    PrimaryVolumeDescriptor pvd;
    if (!find_pvd(iso_file, &pvd)) {
        fclose(iso_file);
        return 1;
    }

    printf("--- Procurando por '%s' ---\n", path);
    DirectoryEntry *dir_entry = find_directory_entry(iso_file, &pvd, path);
    if (!dir_entry) {
        fprintf(stderr, "Erro: Nao foi possivel encontrar o caminho '%s'.\n", path);
        fclose(iso_file);
        return 1;
    }

    if (strcmp(mode, "extract") == 0) {
        if (filter_ext) {
            printf("--- Extraindo arquivos do tipo '%s' de '%s' ---\n\n", filter_ext, path);
        } else {
            printf("--- Extraindo todos os arquivos de '%s' ---\n\n", path);
        }
        extract_directory(iso_file, dir_entry, filter_ext);
    } else {
        if (filter_ext) {
            printf("--- Listando arquivos do tipo '%s' em '%s' ---\n\n", filter_ext, path);
        } else {
            printf("--- Listando todo o conteudo de '%s' ---\n\n", path);
        }
        list_directory_contents(iso_file, dir_entry, filter_ext);
    }

    free(dir_entry);
    fclose(iso_file);
    return 0;
}
