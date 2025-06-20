/**
 * @file systemOp.c
 * @brief Implementação das funções de baixo nível para manipulação do sistema de arquivos Ext2.
 *
 * Este arquivo contém a lógica para ler e escrever as estruturas fundamentais do Ext2,
 * como o superbloco e os descritores de grupo. Ele serve como a camada de abstração
 * entre o shell e o disco (imagem do sistema de arquivos).
 *
 * Baseado nos protótipos de headers.h e inspirado na lógica de um shell Ext2 completo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "headers.h"

// A localização padrão (offset) do superbloco na imagem do disco.
#define SUPERBLOCO_OFFSET 1024

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

    // 1. Checagem fundamental: O número mágico
    if (sb->magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Erro de validação: Assinatura mágica inválida (esperado %#x, encontrado %#x).\n",
                EXT2_SUPER_MAGIC, sb->magic);
        return 0; // Inválido
    }

    // 2. Checagem de consistência das contagens
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
    
    // 3. Checagem da geometria dos grupos
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

    // 4. Validação do tamanho do bloco
    uint32_t block_size = 1024 << sb->log_block_size;
    if (block_size < EXT2_MIN_BLOCK_SIZE || block_size > EXT2_MAX_BLOCK_SIZE) {
        fprintf(stderr, "Erro de validação: Tamanho de bloco inválido (%u bytes).\n", block_size);
        return 0; // Inválido
    }
    
    // 5. Validação do tamanho do inode
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
 * @return Ponteiro para o array de descritores de grupo, ou NULL em caso de erro.
 */
group_desc* ler_descritores_grupo(int fd, const superbloco* sb, uint32_t* num_grupos_out) {
    if (!sb || !num_grupos_out) {
        fprintf(stderr, "Erro (ler_descritores_grupo): Argumentos nulos fornecidos.\n");
        return NULL;
    }

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    // A GDT começa no bloco seguinte ao superbloco.
    // Se block_size = 1024, SB está no bloco 1, GDT no bloco 2 (offset 2048).
    // Se block_size > 1024, SB está no bloco 0, GDT no bloco 1.
    // O campo s_first_data_block nos dá o primeiro bloco de dados, que sempre vem depois da GDT.
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


#include <time.h> // Necessário para a função print_inode

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

    // 1. Descobrir a qual grupo de blocos o inode pertence.
    uint32_t grupo_idx = (inode_num - 1) / sb->inodes_per_group;

    // 2. Obter o descritor desse grupo.
    const group_desc* gd = &gdt[grupo_idx];

    // 3. Obter o início da tabela de inodes para aquele grupo.
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    off_t inicio_tabela_inodes = (off_t)gd->inode_table * tamanho_bloco;

    // 4. Calcular o índice do inode dentro do seu grupo.
    uint32_t indice_no_grupo = (inode_num - 1) % sb->inodes_per_group;
    
    // 5. Calcular o offset final do inode.
    uint16_t tamanho_inode = obter_tamanho_inode(sb);
    off_t offset_final_inode = inicio_tabela_inodes + (indice_no_grupo * tamanho_inode);

    // 6. Posicionar o cursor e ler o inode.
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
    // Calcula em qual byte do array o nosso bit está.
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
    // A máscara invertida terá 0 na posição do nosso bit e 1 em todas as outras.
    // Ex: ~(00001000) -> 11110111
    // Ao usar o operador E (AND) com essa máscara invertida, forçamos o nosso
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