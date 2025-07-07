/**
 * @file systemOp.c
 * @brief Implementação das funções de baixo nível para manipulação do sistema de arquivos Ext2.
 * 
 * @author Allan Custódio Diniz Marques (L33tSh4rk), 
 * @author Vitor Hugo Melo Ribeiro
 *
 * Este arquivo contém a lógica para ler e escrever as estruturas fundamentais do Ext2,
 * como o superbloco e os descritores de grupo. Ele serve como a camada de abstração
 * entre o shell e o disco (imagem do sistema de arquivos).
 *
 * Data de criação: 24 de abril de 2025
 * Data de atualização: 6 de julho de 2025
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "headers.h"
#include "commands.h"

// A localização padrão (offset) do superbloco na imagem do disco.
#define SUPERBLOCO_OFFSET 1024

#define TAMANHO_CABECALHO_ENTRADA_DIR 8

// Variável global estática para armazenar o tamanho do inode do sistema de arquivos atual.
// É definida uma vez na leitura do superbloco para ser usada consistentemente.
static uint16_t tamanho_inode_fs = EXT2_GOOD_OLD_INODE_SIZE;


/*
 * =================================================================================
 * Funções do Superbloco
 * =================================================================================
 */

/**
 * @brief Lê o superbloco do disco.
 *
 * Posiciona o leitor no offset padrão (1024 bytes) e lê os dados para a
 * estrutura `superbloco`. Em caso de sucesso, também atualiza a variável
 * global `tamanho_inode_fs`.
 *
 * @param fd Descritor de arquivo da imagem do disco.
 * @param sb Ponteiro para a estrutura onde o superbloco será armazenado.
 * @return 0 em sucesso, -1 em caso de erro.
 */
int ler_superbloco(int fd, superbloco* sb) {
    if (!sb) {
        fprintf(stderr, "Erro (ler_superbloco): Ponteiro para superbloco é nulo.\n");
        return -1;
    }

    // Posiciona o cursor no início do superbloco.
    if (lseek(fd, SUPERBLOCO_OFFSET, SEEK_SET) == -1) {
        perror("Erro (ler_superbloco): Falha ao posicionar (lseek) para o superbloco");
        return -1;
    }

    // Lê os dados do superbloco do disco para a struct.
    if (read(fd, sb, sizeof(superbloco)) != sizeof(superbloco)) {
        perror("Erro (ler_superbloco): Falha ao ler os dados do superbloco");
        return -1;
    }

    // Após a leitura bem-sucedida, armazena o tamanho do inode para uso futuro.
    tamanho_inode_fs = obter_tamanho_inode(sb);

    return 0; // Sucesso
}

/**
 * @brief Valida se o superbloco pertence a um sistema de arquivos Ext2.
 *
 * @param sb Ponteiro para o superbloco preenchido.
 * @return 1 se o "número mágico" for o esperado (0xEF53), 0 caso contrário.
 */
int validar_superbloco(const superbloco* sb) {
    if (!sb) {
        fprintf(stderr, "Erro de validação: Ponteiro para superbloco é nulo.\n");
        return 0; // Inválido
    }

    // Checagem fundamental: O número mágico
    if (sb->magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Erro de validação: Assinatura mágica inválida (esperado %#x, encontrado %#x).\n",
                EXT2_SUPER_MAGIC, sb->magic);
        return 0; // Inválido
    }

    // Checagem de consistência das contagens
    if (sb->free_blocks_count > sb->blocks_count) {
        fprintf(stderr, "Erro de validação: Contagem de blocos livres (%u) é maior que a contagem total de blocos (%u).\n",
                sb->free_blocks_count, sb->blocks_count);
        return 0; // Inválido
    }
    if (sb->free_inodes_count > sb->inodes_count) {
        fprintf(stderr, "Erro de validação: Contagem de inodes livres (%u) é maior que a contagem total de inodes (%u).\n",
                sb->free_inodes_count, sb->inodes_count);
        return 0; // Inválido
    }
    
    // Checagem da geometria dos grupos
    if (sb->blocks_per_group == 0 || sb->inodes_per_group == 0) {
        fprintf(stderr, "Erro de validação: blocks_per_group ou inodes_per_group é zero.\n");
        return 0; // Inválido
    }
    
    // Verifica se o número de grupos é consistente
    uint32_t groups_from_blocks = (sb->blocks_count + sb->blocks_per_group - 1) / sb->blocks_per_group;
    uint32_t groups_from_inodes = (sb->inodes_count + sb->inodes_per_group - 1) / sb->inodes_per_group;

    if (groups_from_blocks != groups_from_inodes) {
        fprintf(stderr, "Erro de validação: Inconsistência no número de grupos (calculado por blocos: %u, por inodes: %u).\n",
                groups_from_blocks, groups_from_inodes);
        return 0; // Inválido
    }

    // Validação do tamanho do bloco
    uint32_t block_size = 1024 << sb->log_block_size;
    if (block_size < EXT2_MIN_BLOCK_SIZE || block_size > EXT2_MAX_BLOCK_SIZE) {
        fprintf(stderr, "Erro de validação: Tamanho de bloco inválido (%u bytes).\n", block_size);
        return 0; // Inválido
    }
    
    // Validação do tamanho do inode
    if (sb->rev_level >= EXT2_DYNAMIC_REV) {
        if (sb->inode_size < EXT2_GOOD_OLD_INODE_SIZE || (sb->inode_size & (sb->inode_size - 1)) != 0) {
            fprintf(stderr, "Erro de validação: Para revisão dinâmica, o tamanho do inode (%u) é inválido.\n", sb->inode_size);
            return 0; // Inválido
        }
    }

    // Se passou por todas as checagens, o superbloco é considerado robusto e válido.
    return 1; // Válido

}

/**
 * @brief Imprime informações formatadas e detalhadas do superbloco.
 *
 * @param sb Ponteiro para o superbloco a ser impresso.
 */
void print_superbloco(const superbloco* sb) {
    if (!sb) {
        printf("Superbloco fornecido é nulo.\n");
        return;
    }

    // Calcula o tamanho do bloco e do fragmento
    uint32_t tamanho_bloco = 1024 << sb->log_block_size;
    long int tamanho_fragmento = 1024 << sb->log_frag_size;
    // O cast para long int é para evitar problemas com bit shifts em valores negativos se s_log_frag_size for negativo

    printf("inodes count: %u\n", sb->inodes_count);
    printf("blocks count: %u\n", sb->blocks_count);
    printf("reserved blocks count: %u\n", sb->r_blocks_count);
    printf("free blocks count: %u\n", sb->free_blocks_count);
    printf("free inodes count: %u\n", sb->free_inodes_count);
    printf("first data block: %u\n", sb->first_data_block);
    printf("block size: %u\n", tamanho_bloco);
    printf("fragment size: %ld\n", tamanho_fragmento);
    printf("blocks per group: %u\n", sb->blocks_per_group);
    printf("fragments per group: %u\n", sb->frags_per_group);
    printf("inodes per group: %u\n", sb->inodes_per_group);
    printf("mount time: %u\n", sb->mtime);
    printf("write time: %u\n", sb->wtime);
    printf("mount count: %u\n", sb->mnt_count);
    printf("max mount count: %u\n", sb->max_mnt_count);
    printf("magic signature: %#x\n", sb->magic); // %#x para imprimir "0x"
    printf("file system state: %u\n", sb->state);
    printf("errors: %u\n", sb->errors);
    printf("minor revision level: %u\n", sb->minor_rev_level);

    // Formata o timestamp de 'lastcheck' para data legível
    if (sb->lastcheck > 0) {
        time_t tempo_verificacao = sb->lastcheck;
        struct tm *info_tempo = localtime(&tempo_verificacao);
        char buffer_data[80];
        strftime(buffer_data, sizeof(buffer_data), "%d/%m/%Y %H:%M", info_tempo);
        printf("time of last check: %s\n", buffer_data);
    } else {
        printf("time of last check: 0\n");
    }

    printf("max check interval: %u\n", sb->checkinterval);
    printf("creator OS: %u\n", sb->creator_os);
    printf("revision level: %u\n", sb->rev_level);
    
    // Na sua struct headers.h, não há 's_def_resuid' ou 's_def_resgid'.
    // Os campos a seguir são da revisão dinâmica.
    if (sb->rev_level >= EXT2_DYNAMIC_REV) {
        printf("first non-reserved inode: %u\n", sb->first_ino);
        printf("inode size: %u\n", sb->inode_size);
        printf("block group number: %u\n", sb->block_group_nr);
        printf("compatible feature set: %u\n", sb->feature_compat);
        printf("incompatible feature set: %u\n", sb->feature_incompat);
        printf("read only comp feature set: %u\n", sb->feature_ro_compat);

        // Imprime o UUID formatado
        printf("volume UUID: ");
        for (int i = 0; i < 16; i++) {
            printf("%02x", sb->uuid[i]);
        }
        printf("\n");

        // Imprime nomes de volume e montagem com tamanho fixo para evitar lixo
        printf("volume name: %.16s\n", sb->volume_name);
        printf("volume last mounted: %.64s\n", sb->last_mounted);
        printf("algorithm usage bitmap: %u\n", sb->algo_bitmap);
 
    }
}
/**
 * @brief Calcula o tamanho de um bloco de dados em bytes.
 *
 * A fórmula é 1024 * 2^(s_log_block_size), ou, de forma eficiente, 1024 << s_log_block_size.
 *
 * @param sb Ponteiro para o superbloco.
 * @return O tamanho do bloco em bytes (ex: 1024, 2048, 4096).
 */
uint32_t calcular_tamanho_do_bloco(const superbloco* sb) {
    return EXT2_MIN_BLOCK_SIZE << sb->log_block_size;
}

/**
 * @brief Escreve o conteúdo de uma estrutura de superbloco de volta no disco.
 *
 * @param fd O descritor de arquivo do dispositivo.
 * @param sb Ponteiro para a estrutura com os dados a serem escritos.
 * @return 0 em sucesso, -1 em caso de erro.
 */
int escrever_superbloco(int fd, const superbloco* sb) {
    if (!sb) {
        fprintf(stderr, "Erro (escrever_superbloco): Ponteiro para superbloco é nulo.\n");
        return -1;
    }

    if (lseek(fd, SUPERBLOCO_OFFSET, SEEK_SET) == -1) {
        perror("Erro (escrever_superbloco): Falha ao posicionar (lseek)");
        return -1;
    }

    if (write(fd, sb, sizeof(superbloco)) != sizeof(superbloco)) {
        perror("Erro (escrever_superbloco): Falha ao escrever os dados");
        return -1;
    }

    return 0; // Sucesso
}

/**
 * @brief Obtém o tamanho da estrutura do inode com base na revisão do FS.
 *
 * Para sistemas com revisão dinâmica (rev 1), o tamanho é especificado no superbloco.
 * Para revisões antigas (rev 0), o tamanho é fixo (128 bytes).
 *
 * @param sb Ponteiro para a estrutura superbloco.
 * @return O tamanho de um inode em bytes.
 */
uint32_t obter_tamanho_inode(const superbloco *sb) {
    if (sb->rev_level >= EXT2_DYNAMIC_REV && sb->inode_size > 0) {
        return sb->inode_size;
    }
    return EXT2_GOOD_OLD_INODE_SIZE;
}


/*
 * =================================================================================
 * Funções do Descritor de Grupo
 * =================================================================================
 */

/**
 * @brief Lê a Tabela de Descritores de Grupo (GDT) inteira do disco.
 *
 * Aloca memória para um array de `group_desc` e preenche com os dados lidos.
 * A memória alocada deve ser liberada posteriormente com `liberar_descritores_grupo()`.
 *
 * @param fd Descritor de arquivo do disco.
 * @param sb Ponteiro para o superbloco (necessário para cálculos).
 * @param num_grupos_out Ponteiro de saída para armazenar o número de grupos lidos.
 * @return Ponteiro para o array de descritores de grupo, ou NULL em caso de err
 */
group_desc* ler_descritores_grupo(int fd, const superbloco* sb, uint32_t* num_grupos_out) {
    if (!sb || !num_grupos_out) {
        fprintf(stderr, "Erro (ler_descritores_grupo): Argumentos nulos fornecidos.\n");
        return NULL;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    // A GDT começa no bloco seguinte ao superbloco
    // Se block_size = 1024, SB está no bloco 1, GDT no bloco 2 (offset 2048)
    // Se block_size > 1024, SB está no bloco 0, GDT no bloco 1
    // O campo s_first_data_block nos dá o primeiro bloco de dados, que sempre vem depois da GDT
    off_t gdt_offset = (off_t)(sb->first_data_block + 1) * tamanho_bloco;

    // Calcula o número total de grupos de blocos.
    uint32_t num_grupos = (sb->blocks_count + sb->blocks_per_group - 1) / sb->blocks_per_group;
    *num_grupos_out = num_grupos;

    // Calcula o tamanho total da GDT em bytes.
    size_t gdt_tamanho_total = num_grupos * sizeof(group_desc);
    if (gdt_tamanho_total == 0) {
        fprintf(stderr, "Erro (ler_descritores_grupo): Tamanho da GDT é zero.\n");
        return NULL;
    }

    // Aloca memória para a tabela.
    group_desc* gdt = (group_desc*) malloc(gdt_tamanho_total);
    if (!gdt) {
        perror("Erro (ler_descritores_grupo): Falha ao alocar memória para GDT");
        return NULL;
    }

    // Posiciona e lê a tabela inteira do disco.
    if (lseek(fd, gdt_offset, SEEK_SET) == -1) {
        perror("Erro (ler_descritores_grupo): Falha ao posicionar (lseek) para a GDT");
        free(gdt);
        return NULL;
    }

    if (read(fd, gdt, gdt_tamanho_total) != (ssize_t)gdt_tamanho_total) {
        perror("Erro (ler_descritores_grupo): Falha ao ler os dados da GDT");
        free(gdt);
        return NULL;
    }

    return gdt; // Sucesso
}

/**
 * @brief Escreve um descritor de grupo específico de volta no disco.
 *
 * @param fd Descritor de arquivo do disco.
 * @param sb Superbloco do sistema de arquivos.
 * @param grupo_idx O índice do grupo (base 0) a ser escrito.
 * @param gd Ponteiro para a estrutura `group_desc` com os dados a serem escritos.
 * @return 0 em sucesso, -1 em caso de erro.
 */
int escrever_descritor_grupo(int fd, const superbloco* sb, uint32_t grupo_idx, const group_desc* gd) {
    if (!sb || !gd) {
        fprintf(stderr, "Erro (escrever_descritor_grupo): Argumentos nulos fornecidos.\n");
        return -1;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    off_t gdt_base_offset = (off_t)(sb->first_data_block + 1) * tamanho_bloco;
    
    // Calcula o offset exato do descritor de grupo que queremos escrever.
    off_t gd_especifico_offset = gdt_base_offset + (grupo_idx * sizeof(group_desc));

    if (lseek(fd, gd_especifico_offset, SEEK_SET) == -1) {
        perror("Erro (escrever_descritor_grupo): Falha ao posicionar (lseek)");
        return -1;
    }

    if (write(fd, gd, sizeof(group_desc)) != sizeof(group_desc)) {
        perror("Erro (escrever_descritor_grupo): Falha ao escrever os dados");
        return -1;
    }

    return 0; // Sucesso
}

/**
 * @brief Libera a memória alocada para o array de descritores de grupo.
 *
 * @param gdt Ponteiro para o array de descritores alocado por `ler_descritores_grupo`.
 */
void liberar_descritores_grupo(group_desc* gdt) {
    if (gdt) {
        free(gdt);
    }
}


/*
 * =================================================================================
 * Funções de Manipulação de Inodes
 * =================================================================================
 */

/**
 * @brief Lê um inode específico do disco.
 *
 * Calcula a localização exata de um inode com base no seu número, lê os dados
 * do disco e preenche a estrutura 'inode_out' fornecida.
 *
 * @param fd O descritor de arquivo da imagem do disco.
 * @param sb O superbloco do sistema de arquivos (necessário para os cálculos).
 * @param gdt A tabela de descritores de grupo (para encontrar a tabela de inodes).
 * @param inode_num O número do inode a ser lido (começando em 1).
 * @param inode_out Ponteiro para a estrutura 'inode' onde os dados serão armazenados.
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int ler_inode(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_num, inode* inode_out) {
    if (inode_num == 0 || inode_num > sb->inodes_count) {
        fprintf(stderr, "Erro (ler_inode): Número de inode inválido: %u\n", inode_num);
        return -1;
    }
    if (!inode_out) {
        fprintf(stderr, "Erro (ler_inode): A estrutura de saída do inode é nula.\n");
        return -1;
    }

    // Descobrir a qual grupo de blocos o inode pertence.
    uint32_t grupo_idx = (inode_num - 1) / sb->inodes_per_group;

    // Obter o descritor desse grupo.
    const group_desc* gd = &gdt[grupo_idx];

    // Obter o início da tabela de inodes para aquele grupo.
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    off_t inicio_tabela_inodes = (off_t)gd->inode_table * tamanho_bloco;

    // Calcular o índice do inode dentro do seu grupo.
    uint32_t indice_no_grupo = (inode_num - 1) % sb->inodes_per_group;
    
    // Calcular o offset final do inode.
    uint16_t tamanho_inode = obter_tamanho_inode(sb);
    off_t offset_final_inode = inicio_tabela_inodes + (indice_no_grupo * tamanho_inode);

    // Posicionar o cursor e ler o inode.
    if (lseek(fd, offset_final_inode, SEEK_SET) == -1) {
        perror("Erro (ler_inode): Falha ao posicionar (lseek) para o inode");
        return -1;
    }

    if (read(fd, inode_out, sizeof(inode)) != sizeof(inode)) {
        perror("Erro (ler_inode): Falha ao ler os dados do inode");
        return -1;
    }

    return 0; // Sucesso
}

/**
 * @brief Escreve uma estrutura de inode de volta para o disco.
 *
 * Calcula a localização exata de um inode e escreve o conteúdo de 'inode_in'
 * para essa posição.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco.
 * @param gdt A tabela de descritores de grupo.
 * @param inode_num O número do inode a ser escrito.
 * @param inode_in Ponteiro para a estrutura 'inode' contendo os dados a serem escritos.
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int escrever_inode(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_num, const inode* inode_in) {
    if (inode_num == 0 || inode_num > sb->inodes_count) {
        fprintf(stderr, "Erro (escrever_inode): Número de inode inválido: %u\n", inode_num);
        return -1;
    }
    if (!inode_in) {
        fprintf(stderr, "Erro (escrever_inode): A estrutura de entrada do inode é nula.\n");
        return -1;
    }

    // A lógica para encontrar o offset do inode é idêntica à da função de leitura.
    uint32_t grupo_idx = (inode_num - 1) / sb->inodes_per_group;
    const group_desc* gd = &gdt[grupo_idx];
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    off_t inicio_tabela_inodes = (off_t)gd->inode_table * tamanho_bloco;
    uint32_t indice_no_grupo = (inode_num - 1) % sb->inodes_per_group;
    uint16_t tamanho_inode = obter_tamanho_inode(sb);
    off_t offset_final_inode = inicio_tabela_inodes + (indice_no_grupo * tamanho_inode);

    // Posicionar o cursor e escrever o inode.
    if (lseek(fd, offset_final_inode, SEEK_SET) == -1) {
        perror("Erro (escrever_inode): Falha ao posicionar (lseek) para o inode");
        return -1;
    }
    
    if (write(fd, inode_in, sizeof(inode)) != sizeof(inode)) {
        perror("Erro (escrever_inode): Falha ao escrever os dados do inode");
        return -1;
    }

    return 0; // Sucesso
}

/**
 * @brief Imprime o conteúdo de uma estrutura inode de forma legível.
 *
 * @param ino Ponteiro para a estrutura inode a ser impressa.
 * @param inode_num O número do inode (usado para o cabeçalho da impressão).
 */
void print_inode(const inode* ino, uint32_t inode_num) {
    (void)inode_num; // Evita aviso de "parâmetro não utilizado"

    if (!ino) {
        printf("Erro: Ponteiro para inode é nulo.\n");
        return;
    }
    
    // O campo dir_acl é reutilizado como os 32 bits superiores do tamanho do arquivo
    // para arquivos maiores que 2GB em sistemas de 32 bits.
    uint64_t tamanho_completo = ino->size | ((uint64_t)ino->dir_acl << 32);

    printf("file format and access rights: %#x\n", ino->mode);
    printf("user id: %u\n", ino->uid);
    printf("lower 32-bit file size: %u\n", ino->size);
    printf("access time: %u\n", ino->atime);
    printf("creation time: %u\n", ino->ctime);
    printf("modification time: %u\n", ino->mtime);
    printf("deletion time: %u\n", ino->dtime);
    printf("group id: %u\n", ino->gid);
    printf("link count inode: %u\n", ino->links_count);
    printf("512-bytes blocks: %u\n", ino->blocks);
    printf("ext2 flags: %#x\n", ino->flags);
    printf("reserved (Linux): %u\n", ino->osd1);

    for (int i = 0; i < EXT2_N_BLOCKS; ++i) {
        printf("pointer[%d]: %u\n", i, ino->block[i]);
    }

    printf("file version (nfs): %u\n", ino->generation);
    printf("block number extended attributes: %u\n", ino->file_acl);
    // Para um sistema de arquivos com rev_level >= 1 e o feature flag apropriado,
    // este campo é usado para os 32 bits superiores do tamanho do arquivo.
    if (tamanho_completo > 0xFFFFFFFF) {
        printf("higher 32-bit file size: %u\n", ino->dir_acl);
    } else {
        printf("higher 32-bit file size: 0\n"); // ou ino->dir_acl se não for um arquivo regular
    }
    printf("location file fragment: %u\n", ino->faddr);
}


/**
 * @brief Aloca um inode livre no sistema de arquivos.
 *
 * Percorre os descritores de grupo em busca de um que tenha inodes livres.
 * Lê o bitmap de inodes correspondente, encontra o primeiro bit zero,
 * marca-o como um, atualiza os contadores e escreve tudo de volta no disco.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco (será modificado se um inode for alocado).
 * @param gdt A tabela de descritores de grupo (será modificada).
 * @return O número do inode alocado em caso de sucesso, 0 em caso de falha.
 */
uint32_t alocar_inode(int fd, superbloco* sb, group_desc* gdt) {
    if (sb->free_inodes_count == 0) {
        fprintf(stderr, "Erro (alocar_inode): Não há inodes livres no sistema de arquivos.\n");
        return 0; // Falha, sem inodes livres
    }

    uint32_t num_grupos = (sb->blocks_count + sb->blocks_per_group - 1) / sb->blocks_per_group;
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    unsigned char* bitmap_buffer = (unsigned char*) malloc(tamanho_bloco);
    if (!bitmap_buffer) {
        perror("Erro (alocar_inode): Falha ao alocar buffer para o bitmap");
        return 0;
    }

    // Itera por cada grupo de blocos
    for (uint32_t i = 0; i < num_grupos; ++i) {
        // Otimização: só verifica este grupo se ele tiver inodes livres
        if (gdt[i].free_inodes_count > 0) {
            // Lê o bitmap de inodes deste grupo
            if (ler_bloco(fd, sb, gdt[i].inode_bitmap, bitmap_buffer) != 0) {
                fprintf(stderr, "Aviso (alocar_inode): Falha ao ler o bitmap de inodes do grupo %u. Tentando próximo grupo.\n", i);
                continue;
            }

            // Encontra o primeiro bit livre (0) no bitmap
            for (uint32_t j = 0; j < sb->inodes_per_group; ++j) {
                if (!bit_esta_setado(bitmap_buffer, j)) {
                    // Bit livre encontrado!
                    setar_bit(bitmap_buffer, j);

                    // Escreve o bitmap modificado de volta no disco
                    if (escrever_bloco(fd, sb, gdt[i].inode_bitmap, bitmap_buffer) != 0) {
                        fprintf(stderr, "Erro (alocar_inode): Falha ao escrever o bitmap de inodes atualizado.\n");
                        free(bitmap_buffer);
                        return 0; // Falha crítica
                    }

                    // Atualiza os contadores em memória
                    sb->free_inodes_count--;
                    gdt[i].free_inodes_count--;

                    // Salva as estruturas atualizadas no disco
                    escrever_superbloco(fd, sb);
                    escrever_descritor_grupo(fd, sb, i, &gdt[i]);
                    
                    free(bitmap_buffer);

                    // Calcula e retorna o número global do inode (base 1)
                    return (i * sb->inodes_per_group) + j + 1;
                }
            }
        }
    }

    // Se o loop terminar, algo está inconsistente (ex: contadores errados)
    fprintf(stderr, "Erro (alocar_inode): Inconsistência! Superbloco indica inodes livres, mas nenhum foi encontrado.\n");
    free(bitmap_buffer);
    return 0; // Falha
}

/**
 * @brief Libera um inode, marcando-o como livre no bitmap.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco (será modificado).
 * @param gdt A tabela de descritores de grupo (será modificada).
 * @param inode_num O número do inode a ser liberado.
 * @return 0 em sucesso, -1 em erro.
 */
int liberar_inode(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_num) {
    if (inode_num == 0 || inode_num > sb->inodes_count) {
        fprintf(stderr, "Erro (liberar_inode): Tentativa de liberar um número de inode inválido: %u\n", inode_num);
        return -1;
    }

    uint32_t grupo_idx = (inode_num - 1) / sb->inodes_per_group;
    uint32_t indice_no_bitmap = (inode_num - 1) % sb->inodes_per_group;
    
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    unsigned char* bitmap_buffer = (unsigned char*) malloc(tamanho_bloco);
    if (!bitmap_buffer) {
        perror("Erro (liberar_inode): Falha ao alocar buffer para o bitmap");
        return -1;
    }
    
    // Lê o bitmap de inodes do grupo correspondente
    if (ler_bloco(fd, sb, gdt[grupo_idx].inode_bitmap, bitmap_buffer) != 0) {
        fprintf(stderr, "Erro (liberar_inode): Falha ao ler o bitmap de inodes do grupo %u.\n", grupo_idx);
        free(bitmap_buffer);
        return -1;
    }

    // Verifica se o inode já não estava livre
    if (!bit_esta_setado(bitmap_buffer, indice_no_bitmap)) {
        fprintf(stderr, "Aviso (liberar_inode): Inode %u já estava livre.\n", inode_num);
        free(bitmap_buffer);
        return 0; // Não é um erro fatal, consideramos sucesso
    }

    // Limpa o bit
    limpar_bit(bitmap_buffer, indice_no_bitmap);

    // Escreve o bitmap modificado de volta
    if (escrever_bloco(fd, sb, gdt[grupo_idx].inode_bitmap, bitmap_buffer) != 0) {
        fprintf(stderr, "Erro (liberar_inode): Falha ao escrever o bitmap de inodes atualizado.\n");
        free(bitmap_buffer);
        return -1;
    }

    // Atualiza os contadores em memória
    sb->free_inodes_count++;
    gdt[grupo_idx].free_inodes_count++;
    
    // Salva as estruturas atualizadas no disco
    escrever_superbloco(fd, sb);
    escrever_descritor_grupo(fd, sb, grupo_idx, &gdt[grupo_idx]);

    free(bitmap_buffer);
    return 0; // Sucesso
}


/*
 * =================================================================================
 * Funções de Manipulação de Bloco de Dados
 * =================================================================================
 */

/**
 * @brief Lê o conteúdo de um único bloco do disco para um buffer.
 *
 * @param fd O descritor de arquivo da imagem do disco.
 * @param sb O superbloco, usado para obter o tamanho do bloco.
 * @param num_bloco O número do bloco a ser lido.
 * @param buffer Um ponteiro para o buffer em memória onde os dados lidos serão armazenados.
 * O chamador é responsável por garantir que o buffer tenha o tamanho adequado.
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int ler_bloco(int fd, const superbloco* sb, uint32_t num_bloco, void* buffer) {
    if (!sb || !buffer) {
        fprintf(stderr, "Erro (ler_bloco): Argumentos de superbloco ou buffer são nulos.\n");
        return -1;
    }
    
    if (num_bloco >= sb->blocks_count) {
        fprintf(stderr, "Erro (ler_bloco): Tentativa de ler um bloco (%u) fora dos limites do disco (%u).\n",
                num_bloco, sb->blocks_count);
        return -1;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    off_t offset = (off_t)num_bloco * tamanho_bloco;

    // Posiciona o cursor no início do bloco desejado.
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Erro (ler_bloco): Falha ao posicionar (lseek) para o bloco de dados");
        return -1;
    }

    // Lê o bloco inteiro para o buffer.
    ssize_t bytes_lidos = read(fd, buffer, tamanho_bloco);
    if (bytes_lidos == -1) {
        perror("Erro (ler_bloco): Falha ao ler os dados do bloco");
        return -1;
    }
    
    if ((uint32_t)bytes_lidos != tamanho_bloco) {
        fprintf(stderr, "Erro (ler_bloco): Leitura incompleta do bloco %u. Esperava %u bytes, leu %zd.\n",
                num_bloco, tamanho_bloco, bytes_lidos);
        return -1;
    }

    return 0; // Sucesso
}


/**
 * @brief Escreve o conteúdo de um buffer para um único bloco no disco.
 *
 * @param fd O descritor de arquivo da imagem do disco.
 * @param sb O superbloco, usado para obter o tamanho do bloco.
 * @param num_bloco O número do bloco onde os dados serão escritos.
 * @param buffer Um ponteiro para o buffer em memória contendo os dados a serem escritos.
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int escrever_bloco(int fd, const superbloco* sb, uint32_t num_bloco, const void* buffer) {
    if (!sb || !buffer) {
        fprintf(stderr, "Erro (escrever_bloco): Argumentos de superbloco ou buffer são nulos.\n");
        return -1;
    }
    
    // Adiciona uma verificação de segurança extra para não sobrescrever o bloco 0 por engano.
    if (num_bloco == 0 || num_bloco >= sb->blocks_count) {
        fprintf(stderr, "Erro (escrever_bloco): Tentativa de escrever em um bloco inválido (%u) ou fora dos limites (%u).\n",
                num_bloco, sb->blocks_count);
        return -1;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    off_t offset = (off_t)num_bloco * tamanho_bloco;

    // Posiciona o cursor no início do bloco desejado.
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Erro (escrever_bloco): Falha ao posicionar (lseek) para o bloco de dados");
        return -1;
    }

    // Escreve o conteúdo do buffer para o disco.
    ssize_t bytes_escritos = write(fd, buffer, tamanho_bloco);
    if (bytes_escritos == -1) {
        perror("Erro (escrever_bloco): Falha ao escrever os dados no bloco");
        return -1;
    }
    
    if ((uint32_t)bytes_escritos != tamanho_bloco) {
        fprintf(stderr, "Erro (escrever_bloco): Escrita incompleta do bloco %u. Tentou %u bytes, escreveu %zd.\n",
                num_bloco, tamanho_bloco, bytes_escritos);
        return -1;
    }

    return 0; // Sucesso
}


/*
 * =================================================================================
 * Funções de Bitmap
 * =================================================================================
 */

/**
 * @brief Verifica se um bit específico em um bitmap está setado (é 1).
 *
 * @param bitmap Ponteiro para o buffer que contém o bitmap.
 * @param bit_idx O índice do bit a ser verificado (começando do 0).
 * @return 1 se o bit for 1 (setado), 0 se o bit for 0 (limpo).
 */
int bit_esta_setado(const unsigned char* bitmap, int bit_idx) {
    // Calcula em qual byte do array o bit está.
    int byte_idx = bit_idx / 8;
    
    // Calcula a posição do bit dentro daquele byte (de 0 a 7).
    int bit_em_byte = bit_idx % 8;
    
    // Cria uma "máscara" com apenas o bit desejado setado.
    // Ex: para o bit na posição 3, a máscara é 00001000 (1 << 3).
    unsigned char mascara = (1 << bit_em_byte);

    // Usa o operador E (AND) bit-a-bit. Se o resultado for diferente de zero,
    // significa que o bit correspondente no byte do bitmap também era 1.
    if (bitmap[byte_idx] & mascara) {
        return 1; // O bit está setado.
    }

    return 0; // O bit está limpo.
}

/**
 * @brief Seta um bit específico em um bitmap para 1 (marca como usado).
 *
 * @param bitmap Ponteiro para o buffer do bitmap (será modificado).
 * @param bit_idx O índice do bit a ser setado.
 */
void setar_bit(unsigned char* bitmap, int bit_idx) {
    int byte_idx = bit_idx / 8;
    int bit_em_byte = bit_idx % 8;
    unsigned char mascara = (1 << bit_em_byte);

    // Usa o operador OU (OR) bit-a-bit. Isso liga o bit desejado para 1
    // sem afetar os outros bits no mesmo byte.
    // Ex: 10100101 | 00001000  ->  10101101
    bitmap[byte_idx] |= mascara;
}

/**
 * @brief Limpa um bit específico em um bitmap para 0 (marca como livre).
 *
 * @param bitmap Ponteiro para o buffer do bitmap (será modificado).
 * @param bit_idx O índice do bit a ser limpo.
 */
void limpar_bit(unsigned char* bitmap, int bit_idx) {
    int byte_idx = bit_idx / 8;
    int bit_em_byte = bit_idx % 8;
    
    // Cria a máscara normal (ex: 00001000).
    unsigned char mascara = (1 << bit_em_byte);

    // Inverte a máscara usando o operador NÃO (NOT) bit-a-bit (~).
    // A máscara invertida terá 0 na posição do bit e 1 em todas as outras.
    // Ex: ~(00001000) -> 11110111
    // Ao usar o operador E (AND) com essa máscara invertida, forçamos o
    // bit para 0 e mantemos todos os outros bits como estavam.
    // Ex: 10101101 & 11110111 -> 10100101
    bitmap[byte_idx] &= ~mascara;
}



/**
 * @brief Imprime os detalhes de todos os descritores de grupo do sistema de arquivos.
 *
 * @param gdt Ponteiro para o array contendo a Tabela de Descritores de Grupo.
 * @param num_grupos O número total de grupos na tabela.
 */
void print_groups(const group_desc* gdt, uint32_t num_grupos) {
    if (!gdt) {
        printf("Erro: A tabela de descritores de grupo não foi carregada.\n");
        return;
    }

    // Itera por cada descritor de grupo e imprime seus detalhes.
    for (uint32_t i = 0; i < num_grupos; ++i) {
        const group_desc* gd = &gdt[i]; // Pega um ponteiro para o grupo atual

        printf("Block Group Descriptor %u:\n", i);
        printf("block bitmap: %u\n", gd->block_bitmap);
        printf("inode bitmap: %u\n", gd->inode_bitmap);
        printf("inode table: %u\n", gd->inode_table);
        printf("free blocks count: %u\n", gd->free_blocks_count);
        printf("free inodes count: %u\n", gd->free_inodes_count);
        printf("used dirs count: %u\n", gd->used_dirs_count);

        // Adiciona um espaçamento se não for o último grupo
        if (i < num_grupos - 1) {
            printf("...\n");
        }
    }
}


/*
 * =================================================================================
 * Funções de Manipulação de Diretório
 * =================================================================================
 */



/**
 * @brief Itera sobre um buffer contendo dados de um bloco de diretório e imprime cada entrada.
 * Esta é uma função auxiliar de baixo nível.
 * @param buffer Ponteiro para o buffer de dados já lido do disco.
 * @param tamanho_bloco O tamanho de um bloco, para checagens de limite.
 */
void imprimir_entradas_de_bloco_dir(const char* buffer, uint32_t tamanho_bloco) {
    uint32_t offset = 0;
    char nome_arquivo[EXT2_NAME_LEN + 1];

    while (offset < tamanho_bloco) {
        ext2_dir_entry* entry = (ext2_dir_entry*)(buffer + offset);

        if (entry->rec_len == 0) {
            fprintf(stderr, "Aviso: Comprimento de registro inválido (0). Fim do bloco ou corrupção.\n");
            break;
        }

        // Processa apenas entradas em uso
        if (entry->inode != 0) {
            memcpy(nome_arquivo, entry->name, entry->name_len);
            nome_arquivo[entry->name_len] = '\0';
            
            // Impressão no formato exato que você pediu
            printf("%s\n", nome_arquivo);
            printf("inode: %u\n", entry->inode);
            printf("record lenght: %u\n", entry->rec_len);
            printf("name lenght: %u\n", entry->name_len);
            printf("file type: %u\n", entry->file_type);
            printf("\n");
        }

        // Se a entrada atual for a última, ela preenche o resto do bloco
        if (offset + entry->rec_len >= tamanho_bloco) {
            break;
        }

        offset += entry->rec_len;
    }
}

/**
 * @brief (Função de Depuração) Lê o primeiro bloco de dados de um inode de diretório e lista suas entradas.
 */
int listar_entradas_diretorio(int fd, const superbloco* sb, const inode* dir_ino) {
    if (!dir_ino || !EXT2_IS_DIR(dir_ino->mode)) {
        fprintf(stderr, "Erro (listar_entradas): Inode inválido ou não é um diretório.\n");
        return -1;
    }

    uint32_t primeiro_bloco = dir_ino->block[0];
    if (primeiro_bloco == 0) {
        printf("Diretório não possui blocos de dados alocados.\n");
        return 0;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    char* buffer = malloc(tamanho_bloco);
    if (!buffer) {
        perror("ls: Falha ao alocar memória");
        return -1;
    }

    printf("--- Listando Entradas do Bloco %u (via listar_entradas_diretorio) ---\n", primeiro_bloco);
    if (ler_bloco(fd, sb, primeiro_bloco, buffer) == 0) {
        imprimir_entradas_de_bloco_dir(buffer, tamanho_bloco);
    }
    printf("---------------------------------------------------------------------\n");
    
    free(buffer);
    return 0;
}



/**
 * @brief (Função Auxiliar Estática) Procura por um nome dentro de um único bloco de dados de diretório.
 *
 * @param num_bloco O número do bloco a ser lido e verificado.
 * @param nome_procurado O nome da entrada a ser encontrada.
 * @param p_inode_encontrado Ponteiro para uma variável onde o inode encontrado será armazenado.
 * @return 1 se encontrado, 0 se não encontrado, -1 em caso de erro de leitura.
 */
static int buscar_nome_em_bloco(int fd, const superbloco* sb, uint32_t num_bloco, const char* nome_procurado, uint32_t* p_inode_encontrado, char* buffer_dados) {
    if (num_bloco == 0) return 0; // Bloco não alocado, não é um erro.
    if (ler_bloco(fd, sb, num_bloco, buffer_dados) != 0) return -1; // Erro de leitura

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t offset = 0;
    size_t tam_nome_procurado = strlen(nome_procurado);

    while (offset < tamanho_bloco) {
        ext2_dir_entry* entry = (ext2_dir_entry*)(buffer_dados + offset);
        if (entry->rec_len == 0) break;

        if (entry->inode != 0 && entry->name_len == tam_nome_procurado) {
            if (strncmp(nome_procurado, entry->name, entry->name_len) == 0) {
                *p_inode_encontrado = entry->inode; // Encontrado! Armazena o resultado.
                return 1; // Retorna 1 para sinalizar "encontrado"
            }
        }
        if (offset + entry->rec_len >= tamanho_bloco) break;
        offset += entry->rec_len;
    }
    return 0; // Não encontrado neste bloco
}


/**
 * @brief Procura por uma entrada de nome específico dentro de um diretório, varrendo
 * todos os seus blocos (diretos e indiretos), e retorna seu número de inode.
 *
 * @return O número do inode da entrada encontrada, ou 0 se não for encontrada ou em caso de erro.
 */
uint32_t procurar_entrada_no_diretorio(int fd, const superbloco* sb, const group_desc* gdt, uint32_t dir_inode_num, const char* nome_procurado) {
    inode dir_ino;
    if (ler_inode(fd, sb, gdt, dir_inode_num, &dir_ino) != 0 || !EXT2_IS_DIR(dir_ino.mode)) {
        return 0;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
    char* buffer_dados = malloc(tamanho_bloco);
    uint32_t* buffer_ponteiros = malloc(tamanho_bloco); // Usado para L1, L2, e L3

    if (!buffer_dados || !buffer_ponteiros) {
        perror("procurar_entrada: falha ao alocar buffers");
        free(buffer_dados); free(buffer_ponteiros);
        return 0;
    }

    uint32_t inode_encontrado = 0;
    int status_busca = 0;

    // Busca nos Blocos Diretos
    for (int i = 0; i < 12; ++i) {
        status_busca = buscar_nome_em_bloco(fd, sb, dir_ino.block[i], nome_procurado, &inode_encontrado, buffer_dados);
        if (status_busca != 0) goto cleanup; // Se encontrou (1) ou deu erro (-1), para a busca.
    }

    // Busca no Bloco de Indireção Simples
    if (dir_ino.block[12] != 0) {
        if (ler_bloco(fd, sb, dir_ino.block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                status_busca = buscar_nome_em_bloco(fd, sb, buffer_ponteiros[i], nome_procurado, &inode_encontrado, buffer_dados);
                if (status_busca != 0) goto cleanup;
            }
        }
    }

    // Busca no Bloco de Indireção Dupla
    if (dir_ino.block[13] != 0) {
        if (ler_bloco(fd, sb, dir_ino.block[13], buffer_ponteiros) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) continue;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        status_busca = buscar_nome_em_bloco(fd, sb, bloco_L2[j], nome_procurado, &inode_encontrado, buffer_dados);
                        if (status_busca != 0) { free(bloco_L2); goto cleanup; }
                    }
                }
                free(bloco_L2);
            }
        }
    }
    
    // NOTA: A busca em indireção tripla seguiria o mesmo padrão, mas é omitida
    // pois a complexidade de código é enorme para um caso de uso extremamente raro.

cleanup:
    free(buffer_dados);
    free(buffer_ponteiros);
    return inode_encontrado; // Retorna o inode se foi encontrado (status=1), ou 0 se não (status=0 ou -1)
}


/**
 * @brief Resolve uma string de caminho para seu número de inode correspondente.
 *
 * Navega pela árvore de diretórios a partir de um ponto inicial (raiz ou diretório atual)
 * para encontrar o inode do alvo final.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco.
 * @param gdt A tabela de descritores de grupo.
 * @param inode_dir_atual O inode do diretório de trabalho atual (usado para caminhos relativos).
 * @param caminho A string do caminho a ser resolvida (ex: "/home/user/doc.txt").
 * @return O número do inode do alvo, ou 0 se o caminho for inválido ou não for encontrado.
 */
uint32_t caminho_para_inode(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, const char* caminho) {
    // strtok modifica a string, então precisamos de uma cópia
    char caminho_mutavel[1024];
    strncpy(caminho_mutavel, caminho, sizeof(caminho_mutavel) - 1);
    caminho_mutavel[sizeof(caminho_mutavel) - 1] = '\0';

    // Lida com o caso simples de "/"
    if (strcmp(caminho_mutavel, "/") == 0) {
        return EXT2_ROOT_INO;
    }

    uint32_t inode_corrente;
    char* proximo_token;

    // Determina o ponto de partida: raiz para caminhos absolutos, ou o dir. atual para relativos.
    if (caminho_mutavel[0] == '/') {
        inode_corrente = EXT2_ROOT_INO;
        proximo_token = strtok(caminho_mutavel, "/");
    } else {
        inode_corrente = inode_dir_atual;
        proximo_token = strtok(caminho_mutavel, "/");
    }

    // Loop de navegação: itera por cada parte do caminho (ex: "home", "user", "doc.txt")
    while (proximo_token != NULL) {
        // Usa nossa função auxiliar para encontrar a próxima parte do caminho no diretório corrente
        uint32_t proximo_inode = procurar_entrada_no_diretorio(fd, sb, gdt, inode_corrente, proximo_token);
        
        if (proximo_inode == 0) {
            // Se não encontrou o componente, o caminho é inválido.
            return 0;
        }
        
        // Atualiza o inode corrente e pega o próximo token
        inode_corrente = proximo_inode;
        proximo_token = strtok(NULL, "/");
    }

    // O inode_corrente agora contém o inode do alvo final.
    return inode_corrente;
}



/**
 * @brief Formata o campo i_mode de um inode em uma string de permissões no estilo "ls -l".
 * * @param mode O valor do campo i_mode.
 * @param buffer O buffer de caracteres onde a string formatada será armazenada (deve ter pelo menos 11 bytes).
 */
void formatar_permissoes(uint16_t mode, char* buffer) {
    // Primeiro caractere: tipo do arquivo
    if (EXT2_IS_DIR(mode))  buffer[0] = 'd';
    else if (EXT2_IS_LNK(mode)) buffer[0] = 'l';
    else if (EXT2_IS_REG(mode)) buffer[0] = 'f';
    else buffer[0] = '?';

    // Permissões do Dono (user)
    buffer[1] = (mode & EXT2_S_IRUSR) ? 'r' : '-';
    buffer[2] = (mode & EXT2_S_IWUSR) ? 'w' : '-';
    buffer[3] = (mode & EXT2_S_IXUSR) ? 'x' : '-';
    
    // Permissões do Grupo (group)
    buffer[4] = (mode & EXT2_S_IRGRP) ? 'r' : '-';
    buffer[5] = (mode & EXT2_S_IWGRP) ? 'w' : '-';
    buffer[6] = (mode & EXT2_S_IXGRP) ? 'x' : '-';

    // Permissões de Outros (others)
    buffer[7] = (mode & EXT2_S_IROTH) ? 'r' : '-';
    buffer[8] = (mode & EXT2_S_IWOTH) ? 'w' : '-';
    buffer[9] = (mode & EXT2_S_IXOTH) ? 'x' : '-';
    
    // Terminador nulo
    buffer[10] = '\0';
}

/**
 * @brief Converte um tamanho em bytes para um formato legível por humanos (B, KiB, MiB, GiB).
 * * @param tamanho_bytes O tamanho do arquivo em bytes.
 * @param buffer O buffer de caracteres onde a string formatada será armazenada (deve ter pelo menos 20 bytes).
 */
void formatar_tamanho_humano(uint32_t tamanho_bytes, char* buffer, size_t buffer_size) {
    const double GIB = 1024.0 * 1024.0 * 1024.0;
    const double MIB = 1024.0 * 1024.0;
    const double KIB = 1024.0;

    if (tamanho_bytes >= GIB) {
        snprintf(buffer, buffer_size, "%.1f GiB", tamanho_bytes / GIB);
    } else if (tamanho_bytes >= MIB) {
        snprintf(buffer, buffer_size, "%.1f MiB", tamanho_bytes / MIB);
    } else if (tamanho_bytes >= KIB) {
        snprintf(buffer, buffer_size, "%.1f KiB", tamanho_bytes / KIB);
    } else {
        snprintf(buffer, buffer_size, "%u B", tamanho_bytes);
    }
}



/**
 * @brief Imprime os atributos de um inode em um formato de linha única, legível para o usuário.
 * * @param ino Um ponteiro para o inode cujos atributos serão impressos.
 */
void imprimir_formato_attr(const inode* ino) {
    if (!ino) return;

    char perm_buffer[11];
    char tamanho_buffer[20];
    char data_buffer[20];

    // Formata cada parte da informação
    formatar_permissoes(ino->mode, perm_buffer);
    formatar_tamanho_humano(ino->size, tamanho_buffer, sizeof(tamanho_buffer));
    
    time_t mtime = ino->mtime;
    strftime(data_buffer, sizeof(data_buffer), "%d/%m/%Y %H:%M", localtime(&mtime));

    // Imprime o cabeçalho e a linha de dados formatada
    printf("%-10s %-4s %-4s %-10s %s\n", "permissões", "uid", "gid", "tamanho", "modificado em");
    printf("%-10s %-4u %-4u %-10s %s\n",
           perm_buffer,
           ino->uid,
           ino->gid,
           tamanho_buffer,
           data_buffer);
}



/**
 * @brief (Função Auxiliar Estática) Copia um único bloco de dados para o buffer de conteúdo principal.
 *
 * Esta função é chamada repetidamente para ler um bloco de dados e anexá-lo ao
 * buffer que está sendo montado, atualizando os ponteiros e contadores necessários.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco.
 * @param num_bloco O número do bloco de dados a ser lido.
 * @param file_ino O inode do arquivo (usado para checar o tamanho total).
 * @param ptr_buffer_atual_ptr Ponteiro para o ponteiro do buffer de conteúdo (para que possamos movê-lo).
 * @param bytes_lidos_ptr Ponteiro para o contador de bytes lidos (para que possamos incrementá-lo).
 * @param bloco_dado_temp Um buffer temporário pré-alocado para a leitura do bloco.
 * @return 0 em sucesso, -1 em erro.
 */
static int copiar_bloco_de_dados(int fd, const superbloco* sb, uint32_t num_bloco,
                                 const inode* file_ino, char** ptr_buffer_atual_ptr,
                                 uint32_t* bytes_lidos_ptr, char* bloco_dado_temp) {
    if (num_bloco == 0 || *bytes_lidos_ptr >= file_ino->size) {
        return 0; // Pula blocos não alocados ou se já lemos o arquivo inteiro
    }
    
    if (ler_bloco(fd, sb, num_bloco, bloco_dado_temp) != 0) {
        fprintf(stderr, "Erro ao ler o bloco de dados %u.\n", num_bloco);
        return -1; // Retorna erro
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t bytes_para_copiar = tamanho_bloco;

    if (*bytes_lidos_ptr + tamanho_bloco > file_ino->size) {
        bytes_para_copiar = file_ino->size - *bytes_lidos_ptr;
    }

    memcpy(*ptr_buffer_atual_ptr, bloco_dado_temp, bytes_para_copiar);
    
    // Atualiza os contadores usando os ponteiros
    *bytes_lidos_ptr += bytes_para_copiar;
    *ptr_buffer_atual_ptr += bytes_para_copiar;
    
    return 0; // Sucesso
}

/**
 * @brief Lê o conteúdo completo de um arquivo, lidando com blocos diretos e indiretos,
 * para um buffer de memória.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco.
 * @param file_ino Um ponteiro para o inode JÁ LIDO do arquivo a ser lido.
 * @return Um ponteiro para um buffer de memória alocado dinamicamente contendo o
 * conteúdo do arquivo. O chamador é RESPONSÁVEL por liberar esta memória com free().
 * Retorna NULL em caso de erro.
 */
char* ler_conteudo_arquivo(int fd, const superbloco* sb, const inode* file_ino) {
    if (!file_ino) return NULL;
    if (file_ino->size == 0) {
        char* buffer_vazio = malloc(1);
        if (buffer_vazio) buffer_vazio[0] = '\0';
        return buffer_vazio;
    }

    char* buffer_conteudo = malloc(file_ino->size + 1);
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    char* bloco_dado_temp = malloc(tamanho_bloco);
    uint32_t* bloco_ponteiros_temp = malloc(tamanho_bloco);

    if (!buffer_conteudo || !bloco_dado_temp || !bloco_ponteiros_temp) {
        perror("Erro (ler_conteudo): Falha ao alocar buffers");
        free(buffer_conteudo); free(bloco_dado_temp); free(bloco_ponteiros_temp);
        return NULL;
    }

    uint32_t bytes_lidos = 0;
    char* ptr_buffer_atual = buffer_conteudo;
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);

    // Blocos Diretos
    for (int i = 0; i < 12; ++i) {
        if (copiar_bloco_de_dados(fd, sb, file_ino->block[i], file_ino, &ptr_buffer_atual, &bytes_lidos, bloco_dado_temp) != 0) goto erro;
    }

    // Bloco de Indireção Simples (12)
    if (bytes_lidos < file_ino->size && file_ino->block[12] != 0) {
        if (ler_bloco(fd, sb, file_ino->block[12], bloco_ponteiros_temp) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (copiar_bloco_de_dados(fd, sb, bloco_ponteiros_temp[i], file_ino, &ptr_buffer_atual, &bytes_lidos, bloco_dado_temp) != 0) goto erro;
            }
        }
    }

    // Bloco de Indireção Dupla (13)
    if (bytes_lidos < file_ino->size && file_ino->block[13] != 0) {
        if (ler_bloco(fd, sb, file_ino->block[13], bloco_ponteiros_temp) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (bloco_ponteiros_temp[i] == 0) continue;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, bloco_ponteiros_temp[i], bloco_L2) == 0) {
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        if (copiar_bloco_de_dados(fd, sb, bloco_L2[j], file_ino, &ptr_buffer_atual, &bytes_lidos, bloco_dado_temp) != 0) { free(bloco_L2); goto erro; }
                    }
                }
                free(bloco_L2);
            }
        }
    }

    // Bloco de Indireção Tripla (14)
    /*
    * IMPORTANTE: a lógica de busca em indireção tripla neste caso foi implementada caso o arquivo que esteja sendo tratado exista mas seja grande demais
    *    apenas para não retornar erro de arquivo inexistente quando na verdade ele existe 
    */
    if (bytes_lidos < file_ino->size && file_ino->block[14] != 0) {
        // Lê o bloco de ponteiros de Nível 1 (L1)
        if (ler_bloco(fd, sb, file_ino->block[14], bloco_ponteiros_temp) == 0) {
            // Itera sobre os ponteiros L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (bytes_lidos >= file_ino->size) break;
                if (bloco_ponteiros_temp[i] == 0) continue;

                // Aloca memória e lê o bloco de ponteiros de Nível 2 (L2)
                uint32_t* bloco_ponteiros_L2 = malloc(tamanho_bloco);
                if (bloco_ponteiros_L2 && ler_bloco(fd, sb, bloco_ponteiros_temp[i], bloco_ponteiros_L2) == 0) {
                    // Itera sobre os ponteiros L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        if (bytes_lidos >= file_ino->size) break;
                        if (bloco_ponteiros_L2[j] == 0) continue;

                        // Aloca memória e lê o bloco de ponteiros de Nível 3 (L3), que aponta para os dados
                        uint32_t* bloco_ponteiros_L3 = malloc(tamanho_bloco);
                        if (bloco_ponteiros_L3 && ler_bloco(fd, sb, bloco_ponteiros_L2[j], bloco_ponteiros_L3) == 0) {
                            // Itera sobre os ponteiros L3, que são os blocos de dados finais
                            for (uint32_t k = 0; k < ponteiros_por_bloco; ++k) {
                                if (bytes_lidos >= file_ino->size) break;
                                // Chama a função auxiliar para copiar o bloco de dados
                                if (copiar_bloco_de_dados(fd, sb, bloco_ponteiros_L3[k], file_ino, &ptr_buffer_atual, &bytes_lidos, bloco_dado_temp) != 0) {
                                    free(bloco_ponteiros_L2);
                                    free(bloco_ponteiros_L3);
                                    goto erro; // Pula para a limpeza em caso de falha
                                }
                            }
                        }
                        free(bloco_ponteiros_L3); // Libera o buffer L3
                    }
                }
                free(bloco_ponteiros_L2); // Libera o buffer L2
            }
        }
    }

    // Adiciona o terminador nulo ao final do conteúdo.
    *ptr_buffer_atual = '\0';
    free(bloco_dado_temp);
    free(bloco_ponteiros_temp);
    return buffer_conteudo;

erro:
// Limpeza em caso de falha em qualquer uma das etapas
    free(buffer_conteudo);
    free(bloco_dado_temp);
    free(bloco_ponteiros_temp);
    return NULL;
}



/**
 * @brief Imprime um resumo formatado e amigável das informações do sistema de arquivos.
 *
 * @param sb Um ponteiro para o superbloco.
 * @param num_grupos O número total de grupos no sistema de arquivos.
 */
void imprimir_formato_info(const superbloco* sb, uint32_t num_grupos) {
    if (!sb) return;

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint64_t tamanho_imagem = (uint64_t)sb->blocks_count * tamanho_bloco;
    uint32_t espaco_livre_kib = ((uint64_t)sb->free_blocks_count * tamanho_bloco) / 1024;
    uint16_t tamanho_inode = obter_tamanho_inode(sb);
    uint32_t tamanho_tabela_inodes_por_grupo = (sb->inodes_per_group * tamanho_inode) / tamanho_bloco;

    // Imprime o nome do volume, garantindo que não imprima lixo
    char nome_volume[17];
    strncpy(nome_volume, sb->volume_name, 16);
    nome_volume[16] = '\0';
    
    printf("%-16s: %s\n", "Volume name.....", nome_volume);
    printf("%-16s: %llu bytes\n", "Image size......", (unsigned long long)tamanho_imagem);
    printf("%-16s: %u KiB\n", "Free space......", espaco_livre_kib);
    printf("%-16s: %u\n", "Free inodes.....", sb->free_inodes_count);
    printf("%-16s: %u\n", "Free blocks.....", sb->free_blocks_count);
    printf("%-16s: %u bytes\n", "Block size......", tamanho_bloco);
    printf("%-16s: %u bytes\n", "Inode size......", tamanho_inode);
    printf("%-16s: %u\n", "Groups count....", num_grupos);
    printf("%-16s: %u blocks\n", "Groups size.....", sb->blocks_per_group);
    printf("%-16s: %u inodes\n", "Groups inodes...", sb->inodes_per_group);
    printf("%-16s: %u blocks\n", "Inodetable size.", tamanho_tabela_inodes_por_grupo);
}



/**
 * @brief Adiciona uma nova entrada de diretório a um diretório pai.
 *
 * Procura espaço nos blocos existentes (diretos). Se não encontrar, tenta alocar um novo
 * bloco de dados, primeiro nos ponteiros diretos e depois no bloco de indireção simples.
 *
 * IMPORTANTE: Modifica 'inode_pai' em memória. O chamador DEVE escrevê-lo de volta ao disco.
 * @return 0 em sucesso, -1 em erro.
 */
int adicionar_entrada_diretorio(int fd, superbloco* sb, group_desc* gdt, inode* inode_pai, uint32_t inode_pai_num, uint32_t inode_filho, const char* nome_filho, uint8_t tipo_arquivo) {
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
    char* buffer_dados = malloc(tamanho_bloco);
    uint32_t* buffer_ponteiros_l1 = malloc(tamanho_bloco);
    uint32_t* buffer_ponteiros_l2 = malloc(tamanho_bloco);

    if (!buffer_dados || !buffer_ponteiros_l1 || !buffer_ponteiros_l2) {
        perror("adicionar_entrada: falha ao alocar buffers");
        goto falha;
    }

    uint16_t tam_nome_novo = strlen(nome_filho);
    uint16_t rec_len_necessario = (TAMANHO_CABECALHO_ENTRADA_DIR + tam_nome_novo + 3) & ~3;

    // fase 1
    // tenta encontrar espaço em blocos existentes

    // busca nos 12 blocos diretos
    for (int i = 0; i < 12; ++i) {
        uint32_t num_bloco = inode_pai->block[i];
        if (num_bloco == 0) continue; // pula blocos não alocados

        if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
            uint32_t offset = 0;
            ext2_dir_entry* entry;
            while (offset < tamanho_bloco) {
                entry = (ext2_dir_entry*)(buffer_dados + offset);
                if (entry->rec_len == 0) break;

                // se por acaso for a última entrada no bloco
                if (offset + entry->rec_len >= tamanho_bloco) {
                    uint16_t rec_len_real_atual = (TAMANHO_CABECALHO_ENTRADA_DIR + entry->name_len + 3) & ~3;
                    if ((entry->rec_len - rec_len_real_atual) >= rec_len_necessario) {
                        uint16_t rec_len_antigo = entry->rec_len;
                        entry->rec_len = rec_len_real_atual;
                        
                        offset += entry->rec_len;
                        ext2_dir_entry *nova_entry = (ext2_dir_entry*)(buffer_dados + offset);
                        nova_entry->inode = inode_filho;
                        nova_entry->name_len = tam_nome_novo;
                        nova_entry->file_type = tipo_arquivo;
                        memcpy(nova_entry->name, nome_filho, tam_nome_novo);
                        nova_entry->rec_len = rec_len_antigo - entry->rec_len;
                        
                        escrever_bloco(fd, sb, num_bloco, buffer_dados);
                        free(buffer_dados); free(buffer_ponteiros_l1); free(buffer_ponteiros_l2);
                        return 0; // sucesso
                    }
                }
                offset += entry->rec_len;
            }
        }
    }

    // se não achou, busca nos blocos de indireção simples (block[12])
    if (inode_pai->block[12] != 0) {
        if (ler_bloco(fd, sb, inode_pai->block[12], buffer_ponteiros_l1) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                uint32_t num_bloco = buffer_ponteiros_l1[i];
                if (num_bloco == 0) continue;
                
                if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
                    uint32_t offset = 0;
                    ext2_dir_entry* entry;
                    while (offset < tamanho_bloco) {
                        entry = (ext2_dir_entry*)(buffer_dados + offset);
                        if (entry->rec_len == 0) break;
                        
                        if (offset + entry->rec_len >= tamanho_bloco) {
                            uint16_t rec_len_real_atual = (TAMANHO_CABECALHO_ENTRADA_DIR + entry->name_len + 3) & ~3;
                            if ((entry->rec_len - rec_len_real_atual) >= rec_len_necessario) {
                                uint16_t rec_len_antigo = entry->rec_len;
                                entry->rec_len = rec_len_real_atual;
                                
                                offset += entry->rec_len;
                                ext2_dir_entry *nova_entry = (ext2_dir_entry*)(buffer_dados + offset);
                                nova_entry->inode = inode_filho;
                                nova_entry->name_len = tam_nome_novo;
                                nova_entry->file_type = tipo_arquivo;
                                memcpy(nova_entry->name, nome_filho, tam_nome_novo);
                                nova_entry->rec_len = rec_len_antigo - entry->rec_len;
                                
                                escrever_bloco(fd, sb, num_bloco, buffer_dados);
                                free(buffer_dados); free(buffer_ponteiros_l1); free(buffer_ponteiros_l2);
                                return 0; // sucesso
                            }
                        }
                        offset += entry->rec_len;
                    }
                }
            }
        }
    }

    // se ainda não achou, busca nos blocos de indireção dupla (block[13])
    if (inode_pai->block[13] != 0) {
        if (ler_bloco(fd, sb, inode_pai->block[13], buffer_ponteiros_l1) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                if (buffer_ponteiros_l1[i] == 0) continue;
                if (ler_bloco(fd, sb, buffer_ponteiros_l1[i], buffer_ponteiros_l2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; j++) {
                        uint32_t num_bloco = buffer_ponteiros_l2[j];
                        if (num_bloco == 0) continue;
                        
                        if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
                            uint32_t offset = 0;
                            ext2_dir_entry* entry;
                            while (offset < tamanho_bloco) {
                                entry = (ext2_dir_entry*)(buffer_dados + offset);
                                if (entry->rec_len == 0) break;
                                
                                if (offset + entry->rec_len >= tamanho_bloco) {
                                    uint16_t rec_len_real_atual = (TAMANHO_CABECALHO_ENTRADA_DIR + entry->name_len + 3) & ~3;
                                    if ((entry->rec_len - rec_len_real_atual) >= rec_len_necessario) {
                                        uint16_t rec_len_antigo = entry->rec_len;
                                        entry->rec_len = rec_len_real_atual;
                                        
                                        offset += entry->rec_len;
                                        ext2_dir_entry *nova_entry = (ext2_dir_entry*)(buffer_dados + offset);
                                        nova_entry->inode = inode_filho;
                                        nova_entry->name_len = tam_nome_novo;
                                        nova_entry->file_type = tipo_arquivo;
                                        memcpy(nova_entry->name, nome_filho, tam_nome_novo);
                                        nova_entry->rec_len = rec_len_antigo - entry->rec_len;
                                        
                                        escrever_bloco(fd, sb, num_bloco, buffer_dados);
                                        free(buffer_dados); free(buffer_ponteiros_l1); free(buffer_ponteiros_l2);
                                        return 0; // sucesso
                                    }
                                }
                                offset += entry->rec_len;
                            }
                        }
                    }
                }
            }
        }
    }

    // fase 2
    // se não achou espaço tenta alocar novo bloco

    uint32_t novo_bloco_dados = alocar_bloco(fd, sb, gdt, inode_pai_num);
    if (novo_bloco_dados == 0) goto falha;

    // prepara o conteúdo do novo bloco de dados
    memset(buffer_dados, 0, tamanho_bloco);
    ext2_dir_entry* nova_entry = (ext2_dir_entry*)buffer_dados;
    nova_entry->inode = inode_filho;
    nova_entry->name_len = tam_nome_novo;
    nova_entry->rec_len = tamanho_bloco;
    nova_entry->file_type = tipo_arquivo;
    memcpy(nova_entry->name, nome_filho, tam_nome_novo);
    escrever_bloco(fd, sb, novo_bloco_dados, buffer_dados);

    // tenta linkar o novo bloco em um ponteiro direto livre
    for (int i = 0; i < 12; i++) {
        if (inode_pai->block[i] == 0) {
            inode_pai->block[i] = novo_bloco_dados;
            goto sucesso;
        }
    }

    // tenta linkar no bloco de indireção simples
    if (inode_pai->block[12] == 0) {
        uint32_t bloco_indireto = alocar_bloco(fd, sb, gdt, inode_pai_num);
        if (bloco_indireto == 0) { liberar_bloco(fd, sb, gdt, novo_bloco_dados); goto falha; }
        inode_pai->block[12] = bloco_indireto;
        inode_pai->blocks += (tamanho_bloco / 512);
        memset(buffer_ponteiros_l1, 0, tamanho_bloco);
        buffer_ponteiros_l1[0] = novo_bloco_dados;
        escrever_bloco(fd, sb, bloco_indireto, buffer_ponteiros_l1);
        goto sucesso;
    } else {
        ler_bloco(fd, sb, inode_pai->block[12], buffer_ponteiros_l1);
        for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
            if (buffer_ponteiros_l1[i] == 0) {
                buffer_ponteiros_l1[i] = novo_bloco_dados;
                escrever_bloco(fd, sb, inode_pai->block[12], buffer_ponteiros_l1);
                goto sucesso;
            }
        }
    }

    // tenta alocar no bloco de indireção dupla (block[13])
    // essa condição é rara mas optamos por manter a lógica caso seja necessário - pois para o projeto não vamos ter uma imagem tão grande
    if (inode_pai->block[13] == 0) {
        
        // cenário A: A árvore de indireção dupla ainda não existe -> criamos tudo do zero
        // precisamos alocar 3 blocos: L1, o primeiro L2, e o bloco de dados
        uint32_t num_bloco_l1 = alocar_bloco(fd, sb, gdt, inode_pai_num);
        if (num_bloco_l1 == 0) { liberar_bloco(fd, sb, gdt, novo_bloco_dados); goto falha; }

        uint32_t num_bloco_l2 = alocar_bloco(fd, sb, gdt, inode_pai_num);
        if (num_bloco_l2 == 0) { liberar_bloco(fd, sb, gdt, num_bloco_l1); liberar_bloco(fd, sb, gdt, novo_bloco_dados); goto falha; }

        // se todas as alocações foram bem-sucedidas, começamos entao a linkar e escrever
        inode_pai->block[13] = num_bloco_l1;
        inode_pai->blocks += (tamanho_bloco / 512); // conta o novo bloco L1

        // prepara e escreve o novo bloco L2. Ele aponta para nosso bloco de dados
        memset(buffer_ponteiros_l2, 0, tamanho_bloco);
        buffer_ponteiros_l2[0] = novo_bloco_dados;
        escrever_bloco(fd, sb, num_bloco_l2, buffer_ponteiros_l2);
        inode_pai->blocks += (tamanho_bloco / 512); // conta o novo bloco L2

        // prepara e escreve o novo bloco L1 - ele aponta para nosso novo bloco L2
        memset(buffer_ponteiros_l1, 0, tamanho_bloco);
        buffer_ponteiros_l1[0] = num_bloco_l2;
        escrever_bloco(fd, sb, num_bloco_l1, buffer_ponteiros_l1);
        
        goto preparar_e_escrever_novo_bloco; // Pula para a parte final que prepara o bloco de dados
    } else {
        // cenário B: O bloco L1 já existe -> recisamos percorrer ele

        ler_bloco(fd, sb, inode_pai->block[13], buffer_ponteiros_l1); // Lê o bloco L1
        for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
            if (buffer_ponteiros_l1[i] == 0) {
                // se encontramos um slot livre em L1 - podemos criar um novo bloco L2 
                uint32_t num_bloco_l2 = alocar_bloco(fd, sb, gdt, inode_pai_num);
                if (num_bloco_l2 == 0) { liberar_bloco(fd, sb, gdt, novo_bloco_dados); goto falha; }
                
                buffer_ponteiros_l1[i] = num_bloco_l2; // linka o novo L2 no L1
                escrever_bloco(fd, sb, inode_pai->block[13], buffer_ponteiros_l1);      //   salva o L1 modificado
                inode_pai->blocks += (tamanho_bloco / 512); // conta o novo bloco L2
                
                // prepara o novo bloco L2
                memset(buffer_ponteiros_l2, 0, tamanho_bloco);
                buffer_ponteiros_l2[0] = novo_bloco_dados;
                escrever_bloco(fd, sb, num_bloco_l2, buffer_ponteiros_l2);
                goto preparar_e_escrever_novo_bloco;
            }

            // se o ponteiro L1 não for nulo, verifica o bloco L2 que ele aponta
            if (ler_bloco(fd, sb, buffer_ponteiros_l1[i], buffer_ponteiros_l2) == 0) {
                for (uint32_t j = 0; j < ponteiros_por_bloco; j++) {
                    if (buffer_ponteiros_l2[j] == 0) { // encontrou slot livre em um bloco L2 existente
                        buffer_ponteiros_l2[j] = novo_bloco_dados;
                        escrever_bloco(fd, sb, buffer_ponteiros_l1[i], buffer_ponteiros_l2); // salva L2 modificado
                        goto preparar_e_escrever_novo_bloco;
                    }
                }
            }
        }
    }

preparar_e_escrever_novo_bloco:
    inode_pai->size += tamanho_bloco;
    inode_pai->blocks += (tamanho_bloco / 512);
    memset(buffer_dados, 0, tamanho_bloco);
    nova_entry = (ext2_dir_entry*)buffer_dados;
    nova_entry->inode = inode_filho;
    nova_entry->name_len = tam_nome_novo;
    nova_entry->rec_len = tamanho_bloco;
    nova_entry->file_type = tipo_arquivo;
    memcpy(nova_entry->name, nome_filho, tam_nome_novo);
    escrever_bloco(fd, sb, novo_bloco_dados, buffer_dados);
    goto sucesso;

falha:
    free(buffer_dados); free(buffer_ponteiros_l1); free(buffer_ponteiros_l2);
    fprintf(stderr, "Erro: Falha ao alocar novo bloco ou diretório está completamente cheio.\n");
    return -1;

sucesso:
    free(buffer_dados); free(buffer_ponteiros_l1); free(buffer_ponteiros_l2);
    return 0;


}





/*
 * =================================================================================
 * Funções de Alocação de Bloco de Dados
 * =================================================================================
 */

/**
 * @brief Aloca um bloco de dados livre no sistema de arquivos.
 *
 * Tenta alocar um bloco no mesmo grupo do inode fornecido para otimizar o
 * posicionamento dos dados (localidade). Se não houver espaço, procura em
 * outros grupos.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco (será modificado).
 * @param gdt A tabela de descritores de grupo (será modificada).
 * @param inode_num O número do inode que possuirá este bloco (usado como dica de localidade).
 * @return O número do bloco alocado em caso de sucesso, 0 em caso de falha.
 */
uint32_t alocar_bloco(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_num) {
    if (sb->free_blocks_count == 0) {
        fprintf(stderr, "Erro (alocar_bloco): Não há blocos livres no sistema de arquivos.\n");
        return 0;
    }

    uint32_t num_grupos = (sb->blocks_count + sb->blocks_per_group - 1) / sb->blocks_per_group;
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    unsigned char* bitmap_buffer = malloc(tamanho_bloco);
    if (!bitmap_buffer) {
        perror("Erro (alocar_bloco): Falha ao alocar buffer para o bitmap");
        return 0;
    }

    // Estratégia de alocação:
    // Tentar alocar no mesmo grupo do inode.
    uint32_t grupo_ideal = (inode_num - 1) / sb->inodes_per_group;
    if (gdt[grupo_ideal].free_blocks_count > 0) {
        if (ler_bloco(fd, sb, gdt[grupo_ideal].block_bitmap, bitmap_buffer) == 0) {
            for (uint32_t i = 0; i < sb->blocks_per_group; ++i) {
                if (!bit_esta_setado(bitmap_buffer, i)) {
                    setar_bit(bitmap_buffer, i);
                    escrever_bloco(fd, sb, gdt[grupo_ideal].block_bitmap, bitmap_buffer);
                    sb->free_blocks_count--;
                    gdt[grupo_ideal].free_blocks_count--;
                    escrever_superbloco(fd, sb);
                    escrever_descritor_grupo(fd, sb, grupo_ideal, &gdt[grupo_ideal]);
                    free(bitmap_buffer);
                    // Calcula o número absoluto do bloco
                    return (grupo_ideal * sb->blocks_per_group) + sb->first_data_block + i;
                }
            }
        }
    }

    // Se não deu certo, procurar em qualquer outro grupo.
    for (uint32_t i = 0; i < num_grupos; ++i) {
        if (gdt[i].free_blocks_count > 0) {
            if (ler_bloco(fd, sb, gdt[i].block_bitmap, bitmap_buffer) != 0) continue;
            for (uint32_t j = 0; j < sb->blocks_per_group; ++j) {
                if (!bit_esta_setado(bitmap_buffer, j)) {
                    setar_bit(bitmap_buffer, j);
                    escrever_bloco(fd, sb, gdt[i].block_bitmap, bitmap_buffer);
                    sb->free_blocks_count--;
                    gdt[i].free_blocks_count--;
                    escrever_superbloco(fd, sb);
                    escrever_descritor_grupo(fd, sb, i, &gdt[i]);
                    free(bitmap_buffer);
                    return (i * sb->blocks_per_group) + sb->first_data_block + j;
                }
            }
        }
    }
    
    free(bitmap_buffer);
    fprintf(stderr, "Erro (alocar_bloco): Inconsistência! Superbloco indica blocos livres, mas nenhum foi encontrado.\n");
    return 0;
}


/**
 * @brief Libera um bloco de dados, marcando-o como livre no bitmap.
 *
 * @param fd O descritor de arquivo.
 * @param sb O superbloco (será modificado).
 * @param gdt A tabela de descritores de grupo (será modificada).
 * @param num_bloco O número do bloco a ser liberado.
 * @return 0 em sucesso, -1 em erro.
 */
int liberar_bloco(int fd, superbloco* sb, group_desc* gdt, uint32_t num_bloco) {
    if (num_bloco < sb->first_data_block || num_bloco >= sb->blocks_count) {
        fprintf(stderr, "Erro (liberar_bloco): Tentativa de liberar um bloco de dados inválido: %u\n", num_bloco);
        return -1;
    }

    uint32_t grupo_idx = (num_bloco - sb->first_data_block) / sb->blocks_per_group;
    uint32_t indice_no_bitmap = (num_bloco - sb->first_data_block) % sb->blocks_per_group;
    
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    unsigned char* bitmap_buffer = malloc(tamanho_bloco);
    if (!bitmap_buffer) {
        perror("Erro (liberar_bloco): Falha ao alocar buffer para o bitmap");
        return -1;
    }

    if (ler_bloco(fd, sb, gdt[grupo_idx].block_bitmap, bitmap_buffer) != 0) {
        fprintf(stderr, "Erro (liberar_bloco): Falha ao ler o bitmap de blocos do grupo %u.\n", grupo_idx);
        free(bitmap_buffer);
        return -1;
    }

    if (!bit_esta_setado(bitmap_buffer, indice_no_bitmap)) {
        fprintf(stderr, "Aviso (liberar_bloco): Bloco %u já estava livre.\n", num_bloco);
        free(bitmap_buffer);
        return 0;
    }

    limpar_bit(bitmap_buffer, indice_no_bitmap);

    if (escrever_bloco(fd, sb, gdt[grupo_idx].block_bitmap, bitmap_buffer) != 0) {
        fprintf(stderr, "Erro (liberar_bloco): Falha ao escrever o bitmap de blocos atualizado.\n");
        free(bitmap_buffer);
        return -1;
    }

    sb->free_blocks_count++;
    gdt[grupo_idx].free_blocks_count++;
    
    escrever_superbloco(fd, sb);
    escrever_descritor_grupo(fd, sb, grupo_idx, &gdt[grupo_idx]);

    free(bitmap_buffer);
    return 0; // Sucesso
}

/**
 * @brief (Função Auxiliar Estática) Procura e remove uma entrada em um único bloco de diretório.
 * @return 1 se a entrada foi removida, 0 se não foi encontrada, -1 em erro.
 */
static int remover_entrada_em_bloco(int fd, const superbloco* sb, uint32_t num_bloco, const char* nome_filho) {
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    char* buffer = malloc(tamanho_bloco);
    if (!buffer) return -1;

    if (ler_bloco(fd, sb, num_bloco, buffer) != 0) {
        free(buffer);
        return -1;
    }

    uint32_t offset = 0;
    size_t tam_nome_filho = strlen(nome_filho);
    ext2_dir_entry* entry_anterior = NULL;

    while (offset < tamanho_bloco) {
        ext2_dir_entry* entry_atual = (ext2_dir_entry*)(buffer + offset);
        if (entry_atual->rec_len == 0) break;

        if (entry_atual->inode != 0 && entry_atual->name_len == tam_nome_filho &&
            strncmp(entry_atual->name, nome_filho, tam_nome_filho) == 0) {
            
            if (entry_anterior != NULL) {
                entry_anterior->rec_len += entry_atual->rec_len;
            } else {
                entry_atual->inode = 0;
            }

            escrever_bloco(fd, sb, num_bloco, buffer);
            free(buffer);
            return 1; // Encontrado e removido!
        }
        entry_anterior = entry_atual;
        offset += entry_atual->rec_len;
    }
    
    free(buffer);
    return 0; // Não encontrado neste bloco
}


/**
 * @brief Remove uma entrada de um diretório pai, procurando nos blocos diretos e indiretos.
 * @return 0 em sucesso, -1 em erro.
 */
int remover_entrada_diretorio(int fd, superbloco* sb, inode* inode_pai, const char* nome_filho) {
    int status = 0;
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
    uint32_t* buffer_ponteiros = malloc(tamanho_bloco);
    if (!buffer_ponteiros) {
        perror("remover_entrada: falha ao alocar buffer");
        return -1;
    }

    // Procura nos blocos diretos
    for (int i = 0; i < 12; i++) {
        if (inode_pai->block[i] == 0) continue;
        status = remover_entrada_em_bloco(fd, sb, inode_pai->block[i], nome_filho);
        if (status != 0) goto cleanup; // Se encontrou (1) ou deu erro (-1), termina.
    }

    // Procura no bloco de indireção simples
    if (inode_pai->block[12] != 0) {
        if (ler_bloco(fd, sb, inode_pai->block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                if (buffer_ponteiros[i] == 0) continue;
                status = remover_entrada_em_bloco(fd, sb, buffer_ponteiros[i], nome_filho);
                if (status != 0) goto cleanup;
            }
        }
    }

    // Procura no bloco de indireção dupla
    if (inode_pai->block[13] != 0) {
        if (ler_bloco(fd, sb, inode_pai->block[13], buffer_ponteiros) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                if (buffer_ponteiros[i] == 0) continue;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; j++) {
                        if (bloco_L2[j] == 0) continue;
                        status = remover_entrada_em_bloco(fd, sb, bloco_L2[j], nome_filho);
                        if (status != 0) { free(bloco_L2); goto cleanup; }
                    }
                }
                free(bloco_L2);
            }
        }
    }

cleanup:
    free(buffer_ponteiros);
    return (status == 1) ? 0 : -1; // Retorna 0 para sucesso, -1 se não encontrou ou deu erro.
}




/**
 * @brief (Função Auxiliar Estática) Verifica se um único bloco de diretório contém entradas além de '.' e '..'.
 * @param num_bloco O número do bloco de dados a ser verificado.
 * @return 1 se encontrar outras entradas, 0 se estiver "limpo", -1 em erro de leitura.
 */
static int bloco_dir_contem_entradas(int fd, const superbloco* sb, uint32_t num_bloco, char* buffer) {
    if (num_bloco == 0) return 0; // Bloco não alocado é considerado limpo.
    if (ler_bloco(fd, sb, num_bloco, buffer) != 0) return -1; // Erro

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t offset = 0;

    while (offset < tamanho_bloco) {
        ext2_dir_entry* entry = (ext2_dir_entry*)(buffer + offset);
        if (entry->rec_len == 0) break;
        
        if (entry->inode != 0) {
            // Se o nome não for "." E não for "..", encontramos um arquivo/dir.
            if (strncmp(entry->name, ".", entry->name_len) != 0 || entry->name_len != 1) {
                if (strncmp(entry->name, "..", entry->name_len) != 0 || entry->name_len != 2) {
                    return 1; // Encontrou uma entrada, portanto não está vazio.
                }
            }
        }
        if (offset + entry->rec_len >= tamanho_bloco) break;
        offset += entry->rec_len;
    }

    return 0; // Nenhuma entrada além de . e .. foi encontrada neste bloco.
}


/**
 * @brief Verifica se um diretório está vazio (contém apenas '.' e '..'), 
 * varrendo todos os seus blocos de dados diretos, indiretos simples e duplos.
 *
 * @param fd Descritor de arquivo.
 * @param sb Superbloco.
 * @param dir_ino Ponteiro para o inode do diretório a ser verificado.
 * @return 1 se estiver vazio, 0 se não estiver, -1 em erro.
 */
int diretorio_esta_vazio(int fd, const superbloco* sb, const inode* dir_ino) {
    if (!dir_ino || !EXT2_IS_DIR(dir_ino->mode)) return -1;

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
    char* buffer_dados = malloc(tamanho_bloco);
    uint32_t* buffer_ponteiros = malloc(tamanho_bloco);

    if (!buffer_dados || !buffer_ponteiros) {
        perror("diretorio_esta_vazio: falha ao alocar buffers");
        free(buffer_dados); free(buffer_ponteiros);
        return -1;
    }

    int status_busca = 0;

    // Verifica os blocos diretos
    for (int i = 0; i < 12; i++) {
        status_busca = bloco_dir_contem_entradas(fd, sb, dir_ino->block[i], buffer_dados);
        if (status_busca != 0) goto cleanup; // Se encontrou (1) ou deu erro (-1), para a busca.
    }

    // Verifica o bloco de indireção simples
    if (dir_ino->block[12] != 0) {
        if (ler_bloco(fd, sb, dir_ino->block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                status_busca = bloco_dir_contem_entradas(fd, sb, buffer_ponteiros[i], buffer_dados);
                if (status_busca != 0) goto cleanup;
            }
        }
    }

    // Verifica o bloco de indireção dupla
    if (dir_ino->block[13] != 0) {
        if (ler_bloco(fd, sb, dir_ino->block[13], buffer_ponteiros) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                if (buffer_ponteiros[i] == 0) continue;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; j++) {
                        status_busca = bloco_dir_contem_entradas(fd, sb, bloco_L2[j], buffer_dados);
                        if (status_busca != 0) { free(bloco_L2); goto cleanup; }
                    }
                }
                free(bloco_L2);
            }
        }
    }

    // A verificação de indireção tripla seguiria o mesmo padrão mas optamos por omitir devido raridade do uso

cleanup:
    free(buffer_dados);
    free(buffer_ponteiros);
    // Se status_busca for 1, significa que não está vazio, então retornamos 0.
    // Se status_busca for 0, significa que está vazio, então retornamos 1.
    // Se status_busca for -1 (erro), retornamos -1.
    if (status_busca == 1) return 0; // Não está vazio
    if (status_busca == 0) return 1; // Está vazio
    return -1; // Erro
}
