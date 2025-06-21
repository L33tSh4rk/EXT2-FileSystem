#ifndef EXT2_HEADERS
#define EXT2_HEADERS

#include <sys/types.h>
#include <stdint.h>

// =================================================================================
// Definições de Constantes e Macros
// =================================================================================

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_ROOT_INO 2          // Inode do diretório raiz
#define EXT2_GOOD_OLD_INODE_SIZE 128
#define EXT2_NAME_LEN 255        // Comprimento máximo do nome de arquivo
#define EXT2_MIN_BLOCK_SIZE 1024
#define EXT2_MAX_BLOCK_SIZE 65536
#define EXT2_MAX_BLOCKS_COUNT 0xFFFFFFFF  // Valor máximo para contagem de blocos
#define EXT2_N_BLOCKS 15

/* Permissões de arquivo */
#define EXT2_S_IRUSR 00400       // Read by owner
#define EXT2_S_IWUSR 00200       // Write by owner
#define EXT2_S_IXUSR 00100       // Execute by owner
#define EXT2_S_IRGRP 00040       // Read by group
#define EXT2_S_IWGRP 00020       // Write by group
#define EXT2_S_IXGRP 00010       // Execute by group
#define EXT2_S_IROTH 00004       // Read by others
#define EXT2_S_IWOTH 00002       // Write by others
#define EXT2_S_IXOTH 00001       // Execute by others

/* Níveis de revisão */
#define EXT2_GOOD_OLD_REV 0      // Revisão original
#define EXT2_DYNAMIC_REV  1      // Revisão dinâmica

/* Tipos de arquivo para entradas de diretório */
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

/* Máscaras para i_mode (tipo e permissões) */
#define EXT2_S_IFMT   0xF000  // Máscara para o tipo de arquivo
#define EXT2_S_IFSOCK 0xC000  // Socket
#define EXT2_S_IFLNK  0xA000  // Link simbólico
#define EXT2_S_IFREG  0x8000  // Arquivo regular
#define EXT2_S_IFBLK  0x6000  // Dispositivo de bloco
#define EXT2_S_IFDIR  0x4000  // Diretório
#define EXT2_S_IFCHR  0x2000  // Dispositivo de caractere
#define EXT2_S_IFIFO  0x1000  // FIFO/Pipe

/* Macros para verificação de tipo */
#define EXT2_IS_REG(mode) (((mode) & EXT2_S_IFMT) == EXT2_S_IFREG)
#define EXT2_IS_DIR(mode) (((mode) & EXT2_S_IFMT) == EXT2_S_IFDIR)
#define EXT2_IS_LNK(mode) (((mode) & EXT2_S_IFMT) == EXT2_S_IFLNK)

// =================================================================================
// Definições de Estruturas (Structs)
// =================================================================================

/*
 * Estrutura do Superbloco
 */
typedef struct {
    /* Offset 0x00 */
    uint32_t inodes_count;          // Número total de inodes
    uint32_t blocks_count;          // Número total de blocos
    uint32_t r_blocks_count;        // Blocos reservados
    uint32_t free_blocks_count;     // Blocos livres
    /* Offset 0x10 */
    uint32_t free_inodes_count;     // Inodes livres
    uint32_t first_data_block;      // Primeiro bloco de dados
    uint32_t log_block_size;        // Log2(block size) - 10 (1024=0)
    uint32_t log_frag_size;         // Log2(fragment size) - 10
    /* Offset 0x20 */
    uint32_t blocks_per_group;      // Blocos por grupo
    uint32_t frags_per_group;       // Fragmentos por grupo
    uint32_t inodes_per_group;      // Inodes por grupo
    uint32_t mtime;                 // Hora da última montagem
    /* Offset 0x30 */
    uint32_t wtime;                 // Hora da última escrita
    uint16_t mnt_count;             // Contador de montagens
    uint16_t max_mnt_count;         // Máximo de montagens
    uint16_t magic;                 // Assinatura (0xEF53)
    uint16_t state;                 // Estado do sistema de arquivos
    /* Offset 0x40 */
    uint16_t errors;                // Comportamento ao detectar erros
    uint16_t minor_rev_level;       // Versão menor
    uint32_t lastcheck;             // Última verificação
    uint32_t checkinterval;         // Intervalo máximo entre verificações
    /* Offset 0x50 */
    uint32_t creator_os;            // Sistema operacional criador
    uint32_t rev_level;             // Nível de revisão
    
    // --- CAMPOS FALTANTES QUE CAUSAVAM O ERRO ---
    uint16_t def_resuid;            // UID padrão para blocos reservados
    uint16_t def_resgid;            // GID padrão para blocos reservados
    // -----------------------------------------

    /* Campos estendidos (só válidos se rev_level >= EXT2_DYNAMIC_REV) */
    /* Offset 0x60 */
    uint32_t first_ino;             // Primeiro inode utilizável
    uint16_t inode_size;            // Tamanho do inode em bytes
    uint16_t block_group_nr;        // Número do grupo deste superbloco
    uint32_t feature_compat;        // Features compatíveis
    /* Offset 0x70 */
    uint32_t feature_incompat;      // Features incompatíveis
    uint32_t feature_ro_compat;     // Features somente leitura
    uint8_t  uuid[16];              // UUID do volume
    /* Offset 0x80 */
    char     volume_name[16];       // Nome do volume
    /* Offset 0x90 */
    char     last_mounted[64];      // Último ponto de montagem
    /* Offset 0xD0 */
    uint32_t algo_bitmap;           // Algoritmos de compressão
    
    // O resto da estrutura até 1024 bytes contém outros campos
    // que podemos ignorar por agora, mas a parte principal está correta.

} __attribute__((packed)) superbloco;

/*
 * Estrutura do Descritor de Grupo de Blocos
 */
typedef struct {
    uint32_t block_bitmap;          // Bloco do bitmap de blocos
    uint32_t inode_bitmap;          // Bloco do bitmap de inodes
    uint32_t inode_table;           // Bloco inicial da tabela de inodes
    uint16_t free_blocks_count;     // Blocos livres neste grupo
    uint16_t free_inodes_count;     // Inodes livres neste grupo
    uint16_t used_dirs_count;       // Diretórios usados neste grupo
    uint16_t _reserved;             // Alinhamento
    uint32_t reserved[3];           // Reservado para futuro uso
} __attribute__((packed)) group_desc;

/*
 * Estrutura do Inode
 */
typedef struct {
    uint16_t mode;                  // Tipo de arquivo e permissões
    uint16_t uid;                   // ID do usuário proprietário
    uint32_t size;                  // Tamanho em bytes
    uint32_t atime;                 // Tempo do último acesso
    uint32_t ctime;                 // Tempo de criação
    uint32_t mtime;                 // Tempo da última modificação
    uint32_t dtime;                 // Tempo de deleção
    uint16_t gid;                   // ID do grupo proprietário
    uint16_t links_count;           // Contagem de links
    uint32_t blocks;                // Número de blocos de 512B alocados
    uint32_t flags;                 // Flags do inode
    uint32_t osd1;                  // Depende do SO (Linux usa para geração)
    uint32_t block[EXT2_N_BLOCKS];  // Ponteiros para blocos de dados
    uint32_t generation;            // Número de geração (NFS)
    uint32_t file_acl;              // ACL do arquivo (obsoleto)
    uint32_t dir_acl;               // ACL do diretório ou tamanho extendido
    uint32_t faddr;                 // Endereço do fragmento (obsoleto)
    uint8_t  osd2[12];              // Depende do SO
} __attribute__((packed)) inode;

/*
 * Estrutura de entrada de diretório
 */
typedef struct {
    uint32_t inode;                 // Número do inode
    uint16_t rec_len;               // Comprimento total da entrada
    uint8_t  name_len;              // Comprimento do nome
    uint8_t  file_type;             // Tipo do arquivo
    char     name[];   // Nome do arquivo (variável)
} __attribute__ ((packed)) ext2_dir_entry;


// =================================================================================
// Protótipos das Funções
// =================================================================================

/* Superbloco */
int ler_superbloco(int fd, superbloco* sb);
int validar_superbloco(const superbloco* sb);
void print_superbloco(const superbloco* sb);
uint32_t calcular_tamanho_do_bloco(const superbloco* sb);
int escrever_superbloco(int fd, const superbloco* sb);
uint32_t obter_tamanho_inode(const superbloco *sb);


/* Descritores de Grupo */
group_desc* ler_descritores_grupo(int fd, const superbloco* sb, uint32_t* num_grupos);
int escrever_descritor_grupo(int fd, const superbloco* sb, uint32_t grupo_idx, const group_desc* gd);
void liberar_descritores_grupo(group_desc* gdt);
void print_groups(const group_desc* gdt, uint32_t num_grupos);

/* Inodes */
int ler_inode(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_num, inode* inode_out);
int escrever_inode(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_num, const inode* inode_in);
void print_inode(const inode* ino, uint32_t inode_num);
uint32_t alocar_inode(int fd, superbloco* sb, group_desc* gdt);
int liberar_inode(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_num);

/* Funções de Manipulação de Bloco de Dados */
int ler_bloco(int fd, const superbloco* sb, uint32_t num_bloco, void* buffer);
int escrever_bloco(int fd, const superbloco* sb, uint32_t num_bloco, const void* buffer);
void imprimir_entradas_de_bloco_dir(const char* buffer, uint32_t tamanho_bloco);

uint32_t alocar_bloco(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_num);
int liberar_bloco(int fd, superbloco* sb, group_desc* gdt, uint32_t num_bloco);



/* Funções Auxiliares de Bitmap */
int bit_esta_setado(const unsigned char* bitmap, int bit_idx);
void setar_bit(unsigned char* bitmap, int bit_idx);
void limpar_bit(unsigned char* bitmap, int bit_idx);

/*Imprime a lista de comandos disponíveis no shell */
void imprimir_ajuda(void);

/*Manipulação de diretórios*/
int listar_entradas_diretorio(int fd, const superbloco* sb, const inode* dir_ino);
uint32_t procurar_entrada_no_diretorio(int fd, const superbloco* sb, const group_desc* gdt, uint32_t dir_inode_num, const char* nome_procurado);
uint32_t caminho_para_inode(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, const char* caminho);
int adicionar_entrada_diretorio(int fd, superbloco* sb, group_desc* gdt, inode* inode_pai, uint32_t inode_pai_num, uint32_t inode_filho, const char* nome_filho, uint8_t tipo_arquivo);
int remover_entrada_diretorio(int fd, superbloco* sb, inode* inode_pai, const char* nome_filho);

/*Formatação*/
void formatar_permissoes(uint16_t mode, char* buffer);
void formatar_tamanho_humano(uint32_t tamanho_bytes, char* buffer, size_t buffer_size);
void imprimir_formato_attr(const inode* ino);

/* Funções de Conteúdo de Arquivo */
char* ler_conteudo_arquivo(int fd, const superbloco* sb, const inode* file_ino);

void imprimir_formato_info(const superbloco* sb, uint32_t num_grupos);


#endif // EXT2_HEADERS