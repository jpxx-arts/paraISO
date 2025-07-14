#ifndef ISO9660_H
#define ISO9660_H

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

#define LOGICAL_SECTOR_SIZE 2048

#pragma pack(push, 1)

/**
 * @brief Formato de data e hora para uma Entrada de Diretório (7 bytes).
 * @note Conforme a seção 10.1.6 do padrão ECMA-119 (6ª Edição, Junho 2024).
 */
typedef struct {
    uint8_t year;       // Anos desde 1900
    uint8_t month;      // Mês (1-12)
    uint8_t day;        // Dia (1-31)
    uint8_t hour;       // Hora (0-23)
    uint8_t minute;     // Minuto (0-59)
    uint8_t second;     // Segundo (0-59)
    int8_t  gmt_offset; // Deslocamento GMT em intervalos de 15 minutos
} DirectoryDateTime;

/**
 * @brief Formato de data e hora para o Descritor de Volume (17 bytes).
 * @note Conforme a seção 9.4.27.2 do padrão ECMA-119 (6ª Edição, Junho 2024).
 */
typedef struct {
    char   year[4];
    char   month[2];
    char   day[2];
    char   hour[2];
    char   minute[2];
    char   second[2];
    char   centiseconds[2];
    int8_t gmt_offset;
} VolumeDateTime;

/**
 * @brief Representa uma Entrada de Diretório (Directory Record).
 * @brief Descreve um arquivo ou um diretório.
 * @note Conforme a seção 10.1.1 do padrão ECMA-119 (6ª Edição, Junho 2024).
 * @note Usa um membro de array flexível para o identificador.
 */
typedef struct {
    uint8_t  length_of_record;
    uint8_t  extended_attribute_length;
    uint32_t extent_location_le;
    uint32_t extent_location_be;
    uint32_t data_length_le;
    uint32_t data_length_be;
    uint8_t  recording_date_time[7];
    uint8_t  file_flags;
    uint8_t  file_unit_size;
    uint8_t  interleave_gap_size;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint8_t  file_identifier_len;
    char     file_identifier[]; // Nome do arquivo/diretório (tamanho variável)
} DirectoryEntry;

/**
 * @brief Representa um registro na Path Table.
 * @brief Cada registro descreve um único diretório no volume.
 * @note Conforme a seção 10.4.1 do padrão ECMA-119 (6ª Edição, Junho 2024).
 */
typedef struct {
    uint8_t  identifier_len;
    uint8_t  extended_attr_len;
    uint32_t extent_location;
    uint16_t parent_dir_number;
    char     directory_identifier[]; // Nome do diretório (tamanho variável)
} PathTableRecord;

/**
 * @brief Descritor de Volume Primário (PVD). Contém as informações principais sobre o volume.
 * @brief Localizado no primeiro setor da Área de Dados (LBA 16).
 * @note Conforme a seção 9.4.1 do padrão ECMA-119 (6ª Edição, Junho 2024).
 */
typedef struct {
    uint8_t  type_code;
    char     standard_identifier[5];
    uint8_t  version;
    uint8_t  _unused1;
    char     system_identifier[32];
    char     volume_identifier[32];
    uint8_t  _unused2[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;
    uint8_t  _unused3[32];
    uint16_t volume_set_size_le;
    uint16_t volume_set_size_be;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint16_t logical_block_size_le;
    uint16_t logical_block_size_be;
    uint32_t path_table_size_le;
    uint32_t path_table_size_be;
    uint32_t loc_path_table_le;
    uint32_t loc_opt_path_table_le;
    uint8_t  _unused4[8]; 
    DirectoryEntry root_directory_entry;
} PrimaryVolumeDescriptor;

#pragma pack(pop)

// --- INTERFACES DE FUNÇÕES COMPARTILHADAS ---

/**
 * @brief Encontra e valida o Descritor de Volume Primário (PVD) no arquivo ISO.
 * @param iso_file Ponteiro para o arquivo ISO aberto.
 * @param pvd Ponteiro para a struct onde o PVD será armazenado.
 * @return true se o PVD foi encontrado e é válido, false caso contrário.
 */
bool find_pvd(FILE *iso_file, PrimaryVolumeDescriptor *pvd);

/**
 * @brief Encontra uma entrada de diretório (arquivo ou pasta) a partir de um caminho completo.
 * @brief Utiliza a Path Table para uma busca eficiente.
 * @param iso_file Ponteiro para o arquivo ISO aberto.
 * @param pvd O PVD já lido do volume.
 * @param path O caminho completo a ser encontrado (ex: /EFI/BOOT/BOOTX64.EFI).
 * @return Um ponteiro para uma DirectoryEntry alocada dinamicamente em caso de sucesso, ou NULL em caso de falha.
 * @note O chamador é responsável por liberar a memória da entrada retornada com free().
 */
DirectoryEntry* find_directory_entry(FILE *iso_file, const PrimaryVolumeDescriptor *pvd, const char *path);

/**
 * @brief Lista o conteúdo (arquivos e subdiretórios) de um diretório na saída padrão.
 * @param iso_file Ponteiro para o arquivo ISO aberto.
 * @param dir_entry Um ponteiro para a DirectoryEntry do diretório a ser listado.
 */
void list_directory_contents(FILE *iso_file, const DirectoryEntry *dir_entry, const char *extension);

/**
 * @brief Extrai recursivamente o conteúdo de um diretório da ISO para o sistema de arquivos local.
 * @param iso_file Ponteiro para o arquivo ISO aberto.
 * @param dir_entry Ponteiro para a DirectoryEntry do diretório a ser extraído.
 * @param extension Filtro opcional de extensão (ex: ".EFI"). Passar NULL para extrair tudo.
 * @note Esta função altera o diretório de trabalho atual (com chdir) para criar a hierarquia de pastas.
 */
void extract_directory(FILE *iso_file, const DirectoryEntry *dir_entry, const char *extension);

/**
 * @brief Exibe o conteúdo de um arquivo da ISO diretamente na saída padrão (terminal).
 * @param iso_file Ponteiro para o arquivo ISO aberto.
 * @param file_entry Ponteiro para a DirectoryEntry do arquivo a ser exibido.
 * @note A entrada fornecida deve ser um arquivo, não um diretório.
 */
void cat_file(FILE *iso_file, const DirectoryEntry *file_entry);

#endif // ISO9660_H
