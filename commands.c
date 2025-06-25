#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h> // Para a função dirname() e basename()
#include <time.h>
#include <stdbool.h>
#include <stddef.h>

#include "commands.h" // Inclui protótipos
#include "headers.h"  // Inclui definições e funções de baixo nível

/**
 * @brief Executa a lógica do comando 'info', que imprime o superbloco.
 */
void comando_info(const superbloco* sb, uint32_t num_grupos) {
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'info' não aceita argumentos.\n");
        return;
    }
    imprimir_formato_info(sb, num_grupos);
}

// =================================================================================
// Implementação dos Comandos
// =================================================================================




// ===================================================== para print ====================================
void comando_print_superblock(const superbloco* sb) {
    // Validação de segurança, caso a função seja chamada incorretamente
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'print superblock' não aceita argumentos adicionais.\n");
        return;
    }
    print_superbloco(sb);
}

void comando_print_inode(int fd, const superbloco* sb, const group_desc* gdt, char* arg_num_inode) {
    if (arg_num_inode == NULL) {
        printf("Uso: print inode <numero>\n");
        return;
    }
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'print inode' recebeu argumentos demais.\n");
        return;
    }

    char* endptr;
    long num_inode_long = strtol(arg_num_inode, &endptr, 10);
    
    if (*endptr != '\0' || num_inode_long <= 0) {
        printf("Erro: O número do inode '%s' é inválido.\n", arg_num_inode);
    } else {
        uint32_t num_inode = (uint32_t)num_inode_long;
        inode ino_para_imprimir;

        if (ler_inode(fd, sb, gdt, num_inode, &ino_para_imprimir) == 0) {
            print_inode(&ino_para_imprimir, num_inode);
        }
        // A função ler_inode() já imprime uma mensagem de erro se falhar
    }
}

void comando_print_groups(const group_desc* gdt, uint32_t num_grupos) {
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'print groups' não aceita argumentos adicionais.\n");
        return;
    }
    print_groups(gdt, num_grupos);
}
// =====================================================================================================


void comando_attr(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("Uso: attr <caminho_para_arquivo_ou_diretorio>\n");
        return;
    }
    
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'attr' recebeu argumentos demais.\n");
        return;
    }


    uint32_t inode_ponto_partida = (caminho_arg[0] == '/') ? EXT2_ROOT_INO : inode_dir_atual;
    
    uint32_t inode_num = caminho_para_inode(fd, sb, gdt, inode_ponto_partida, caminho_arg);
    
    if (inode_num == 0) {
        printf("Erro: Arquivo ou diretório não encontrado: '%s'\n", caminho_arg);
    } else {
        inode ino;
        if (ler_inode(fd, sb, gdt, inode_num, &ino) == 0) {
            imprimir_formato_attr(&ino); 
        } else {
            fprintf(stderr, "Erro crítico ao ler o inode %u.\n", inode_num);
        }
    }
}


void comando_cat(int fd, const superbloco* sb, const group_desc* gdt, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("Uso: cat <caminho_para_arquivo>\n");
        return;
    }
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'cat' recebeu argumentos demais.\n");
        return;
    }

    uint32_t inode_num = caminho_para_inode(fd, sb, gdt, EXT2_ROOT_INO, caminho_arg);
    if (inode_num == 0) {
        printf("cat: %s: Arquivo ou diretório não encontrado\n", caminho_arg);
        return;
    }

    inode ino;
    if (ler_inode(fd, sb, gdt, inode_num, &ino) != 0) {
        fprintf(stderr, "Erro crítico ao tentar ler o inode %u.\n", inode_num);
        return;
    }

    // Valida se é um arquivo regular
    if (!EXT2_IS_REG(ino.mode)) {
        printf("cat: %s: Não é um arquivo regular\n", caminho_arg);
        return;
    }
    
    // Lê o conteúdo do arquivo para a memória
    char* conteudo = ler_conteudo_arquivo(fd, sb, &ino);

    if (conteudo) {
        // Imprime o conteúdo na tela
        printf("%s", conteudo);
        // Libera a memória alocada pela função auxiliar
        free(conteudo);
    } else {
        fprintf(stderr, "Erro ao tentar ler o conteúdo do arquivo.\n");
    }
}



void comando_ls(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg) {
    uint32_t inode_a_listar;
    char* nome_alvo;

    if (caminho_arg == NULL) {
        inode_a_listar = inode_dir_atual;
        nome_alvo = ".";
    } else {
        inode_a_listar = caminho_para_inode(fd, sb, gdt, inode_dir_atual, caminho_arg);
        nome_alvo = caminho_arg;
    }

    if (inode_a_listar == 0) {
        printf("ls: não foi possível acessar '%s': Arquivo ou diretório não encontrado\n", nome_alvo);
        return;
    }

    inode ino;
    if (ler_inode(fd, sb, gdt, inode_a_listar, &ino) != 0) {
        return;
    }

    if (!EXT2_IS_DIR(ino.mode)) {
        printf("%s\n", nome_alvo);
        return;
    }

    // Lógica para os diretórios
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);

    char* buffer_dados = malloc(tamanho_bloco);
    uint32_t* buffer_ponteiros = malloc(tamanho_bloco);

    if (!buffer_dados || !buffer_ponteiros) {
        perror("ls: Falha ao alocar memória para os buffers");
        free(buffer_dados);
        free(buffer_ponteiros);
        return;
    }

    // Itera sobre os 12 ponteiros de blocos diretos
    for (int i = 0; i < 12; ++i) {
        uint32_t num_bloco = ino.block[i];
        if (num_bloco == 0) break;

        if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
            imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
        }
    }

    // Processa o bloco de indireção simples (block[12])
    if (ino.block[12] != 0) {
        if (ler_bloco(fd, sb, ino.block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                uint32_t num_bloco = buffer_ponteiros[i];
                if (num_bloco == 0) break;
                if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
                    imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
                }
            }
        }
    }

    // Processa o bloco de indireção dupla (block[13])
    if (ino.block[13] != 0) {
        if (ler_bloco(fd, sb, ino.block[13], buffer_ponteiros) == 0) { // Lê o bloco de ponteiros L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) break;
                
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê o bloco de ponteiros L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        uint32_t num_bloco = bloco_L2[j];
                        if (num_bloco == 0) break;
                        if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
                            imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
                        }
                    }
                }
                free(bloco_L2);
            }
        }
    }

    // O processamento do bloco de indireção tripla (block[14]) foi omitido por questões de raridade em seu uso.
    

    free(buffer_dados);
    free(buffer_ponteiros);
}

/**
 * @brief Executa a lógica do comando 'pwd', que imprime o diretório de trabalho atual.
 */
void comando_pwd(const char* diretorio_atual_str) {
    // Validação para garantir que o comando não recebeu argumentos extras
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'pwd' não aceita argumentos.\n");
        return;
    }
    printf("%s\n", diretorio_atual_str);
}


/**
 * @brief Executa a lógica do comando 'cd <caminho>', alterando o diretório de trabalho atual.
 *
 * @param p_inode_dir_atual Ponteiro para a variável que guarda o inode do diretório atual.
 * @param diretorio_atual_str Buffer da string que guarda o caminho do diretório atual.
 * @param caminho_arg O caminho de destino fornecido pelo usuário.
 */
void comando_cd(int fd, const superbloco* sb, const group_desc* gdt,
                uint32_t* p_inode_dir_atual, char* diretorio_atual_str,
                char* caminho_arg) {
    if (caminho_arg == NULL) {
        // 'cd' sem argumentos não faz nada
        return;
    }

    // Encontra o inode do diretório de destino
    uint32_t inode_destino = caminho_para_inode(fd, sb, gdt, *p_inode_dir_atual, caminho_arg);

    if (inode_destino == 0) {
        printf("cd: %s: Arquivo ou diretório não encontrado\n", caminho_arg);
        return;
    }

    // Verifica se o destino é realmente um diretório
    inode ino;
    if (ler_inode(fd, sb, gdt, inode_destino, &ino) != 0) {
        return; // Erro já foi impresso por ler_inode
    }
    if (!EXT2_IS_DIR(ino.mode)) {
        printf("cd: %s: Não é um diretório\n", caminho_arg);
        return;
    }

    // Atualiza o estado do shell (o inode e a string do caminho)
    *p_inode_dir_atual = inode_destino;

    // Lógica para atualizar a string do caminho
    if (strcmp(caminho_arg, "..") == 0) {
        // Sobe um nível. Usa dirname para encontrar o diretório pai da string atual.
        // Precisa de uma cópia, pois dirname pode modificar a string.
        char temp_path[1024];
        strncpy(temp_path, diretorio_atual_str, 1024);
        char* parent = dirname(temp_path);
        strcpy(diretorio_atual_str, parent);
    } else if (strcmp(caminho_arg, ".") != 0) {
        // Se o caminho for absoluto, apenas copie-o
        if (caminho_arg[0] == '/') {
            strcpy(diretorio_atual_str, caminho_arg);
        } else {
            // Se for relativo, anexe-o ao caminho atual
            // Adiciona a barra, a menos que já estejamos na raiz
            if (strcmp(diretorio_atual_str, "/") != 0) {
                strcat(diretorio_atual_str, "/");
            }
            strcat(diretorio_atual_str, caminho_arg);
        }
    }
    // Se for 'cd .', não faz nada com a string.

    // Garante que a raiz seja sempre apenas "/"
    if (strlen(diretorio_atual_str) > 1 && diretorio_atual_str[strlen(diretorio_atual_str) - 1] == '/') {
        diretorio_atual_str[strlen(diretorio_atual_str) - 1] = '\0';
    }
}

/**
 * @brief Executa a lógica do comando 'touch', criando um arquivo vazio ou atualizando seu timestamp.
 */
void comando_touch(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("touch: faltando operando de arquivo\n");
        return;
    }
    
    char copia_caminho1[1024], copia_caminho2[1024];
    strncpy(copia_caminho1, caminho_arg, 1024);
    strncpy(copia_caminho2, caminho_arg, 1024);
    char* dir_pai_str = dirname(copia_caminho1);
    char* nome_arquivo_novo = basename(copia_caminho2);

    if (strlen(nome_arquivo_novo) > EXT2_NAME_LEN) {
        printf("touch: nome do arquivo é muito longo\n");
        return;
    }

    uint32_t inode_pai_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, dir_pai_str);
    if (inode_pai_num == 0) {
        printf("touch: diretório pai '%s' não encontrado.\n", dir_pai_str);
        return;
    }
    inode inode_pai;
    if (ler_inode(fd, sb, gdt, inode_pai_num, &inode_pai) != 0 || !EXT2_IS_DIR(inode_pai.mode)) {
        printf("touch: '%s' não é um diretório\n", dir_pai_str);
        return;
    }

    // Verifica se o arquivo já existe. Se sim, imprime o erro e para.
    uint32_t inode_existente_num = procurar_entrada_no_diretorio(fd, sb, gdt, inode_pai_num, nome_arquivo_novo);
    if (inode_existente_num != 0) {
        printf("touch: não foi possível criar o arquivo '%s': Arquivo já existe\n", caminho_arg);
        return;
    }

    uint32_t novo_inode_num = alocar_inode(fd, sb, gdt);
    if (novo_inode_num == 0) {
        printf("touch: Falha ao alocar novo inode.\n");
        return;
    }
    
    if (adicionar_entrada_diretorio(fd, sb, gdt, &inode_pai, inode_pai_num, novo_inode_num, nome_arquivo_novo, EXT2_FT_REG_FILE) != 0) {
        printf("Falha ao adicionar entrada no diretório, revertendo alocação de inode.\n");
        liberar_inode(fd, sb, gdt, novo_inode_num);
        return;
    }

    inode novo_ino;
    memset(&novo_ino, 0, sizeof(inode));
    novo_ino.mode = EXT2_S_IFREG | 0644;
    novo_ino.size = 0;
    novo_ino.links_count = 1;
    novo_ino.atime = novo_ino.mtime = novo_ino.ctime = time(NULL);
    escrever_inode(fd, sb, gdt, novo_inode_num, &novo_ino);
    
    inode_pai.mtime = time(NULL);
    escrever_inode(fd, sb, gdt, inode_pai_num, &inode_pai);

    printf("Arquivo '%s' criado com sucesso.\n", caminho_arg);
}



/**
 * @brief Executa a lógica do comando 'rm', removendo um arquivo.
 */
void comando_rm(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("rm: faltando operando\n");
        return;
    }

    uint32_t inode_alvo_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, caminho_arg);
    if (inode_alvo_num == 0) {
        printf("rm: não foi possível remover '%s': Arquivo não encontrado\n", caminho_arg);
        return;
    }

    inode inode_alvo;
    if (ler_inode(fd, sb, gdt, inode_alvo_num, &inode_alvo) != 0) return;
    if (EXT2_IS_DIR(inode_alvo.mode)) {
        printf("rm: não foi possível remover '%s': É um diretório\n", caminho_arg);
        return;
    }
    
    char copia_caminho[1024];
    strncpy(copia_caminho, caminho_arg, 1024);
    char* nome_arquivo = basename(copia_caminho);
    strncpy(copia_caminho, caminho_arg, 1024);
    char* dir_pai_str = dirname(copia_caminho);
    
    uint32_t inode_pai_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, dir_pai_str);
    inode inode_pai;
    if (ler_inode(fd, sb, gdt, inode_pai_num, &inode_pai) != 0) return;

    if (remover_entrada_diretorio(fd, sb, &inode_pai, nome_arquivo) != 0) {
        printf("rm: erro ao remover a entrada do diretório pai.\n");
        return;
    }

    inode_alvo.links_count--;
    if (inode_alvo.links_count == 0) {
        // Libera todos os blocos de dados
        uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
        uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
        uint32_t* buffer_ponteiros = malloc(tamanho_bloco);

        // Libera blocos diretos
        for (int i = 0; i < 12; i++) {
            if (inode_alvo.block[i] != 0) liberar_bloco(fd, sb, gdt, inode_alvo.block[i]);
        }
        // Libera blocos de indireção simples
        if (inode_alvo.block[12] != 0) {
            if (buffer_ponteiros && ler_bloco(fd, sb, inode_alvo.block[12], buffer_ponteiros) == 0) {
                for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                    if (buffer_ponteiros[i] != 0) liberar_bloco(fd, sb, gdt, buffer_ponteiros[i]);
                }
            }
            liberar_bloco(fd, sb, gdt, inode_alvo.block[12]); // Libera o bloco de ponteiros
        }
        // Libera blocos de indireção dupla
        if (inode_alvo.block[13] != 0) {
            if (buffer_ponteiros && ler_bloco(fd, sb, inode_alvo.block[13], buffer_ponteiros) == 0) { // Lê L1
                for (uint32_t i = 0; i < ponteiros_por_bloco; i++) {
                    if (buffer_ponteiros[i] == 0) continue;
                    uint32_t* bloco_L2 = malloc(tamanho_bloco);
                    if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                        for (uint32_t j = 0; j < ponteiros_por_bloco; j++) {
                            if (bloco_L2[j] != 0) liberar_bloco(fd, sb, gdt, bloco_L2[j]);
                        }
                    }
                    free(bloco_L2);
                    liberar_bloco(fd, sb, gdt, buffer_ponteiros[i]); // Libera o bloco L2
                }
            }
            liberar_bloco(fd, sb, gdt, inode_alvo.block[13]); // Libera o bloco L1
        }
        free(buffer_ponteiros);
        // Lógica de indireção tripla omitida por simplicidade, mas seguiria o mesmo padrão.
        
        liberar_inode(fd, sb, gdt, inode_alvo_num);
        inode_alvo.dtime = time(NULL);
    }

    escrever_inode(fd, sb, gdt, inode_alvo_num, &inode_alvo);
    inode_pai.mtime = inode_pai.atime = time(NULL);
    escrever_inode(fd, sb, gdt, inode_pai_num, &inode_pai);

    printf("Arquivo '%s' removido com sucesso.\n", caminho_arg);
}



/**
 * @brief Executa a lógica do comando 'mkdir', criando um novo diretório.
 */
void comando_mkdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("mkdir: faltando operando\n");
        return;
    }

    // Separar caminho pai e nome do novo diretório
    char copia_caminho1[1024], copia_caminho2[1024];
    strncpy(copia_caminho1, caminho_arg, 1024);
    strncpy(copia_caminho2, caminho_arg, 1024);
    char* dir_pai_str = dirname(copia_caminho1);
    char* nome_dir_novo = basename(copia_caminho2);

    if (strlen(nome_dir_novo) > EXT2_NAME_LEN) {
        printf("mkdir: nome do diretório é muito longo\n");
        return;
    }

    // Encontrar e validar o diretório pai
    uint32_t inode_pai_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, dir_pai_str);
    if (inode_pai_num == 0) {
        printf("mkdir: diretório pai '%s' não encontrado\n", dir_pai_str);
        return;
    }
    inode inode_pai;
    if (ler_inode(fd, sb, gdt, inode_pai_num, &inode_pai) != 0 || !EXT2_IS_DIR(inode_pai.mode)) {
        printf("mkdir: '%s' não é um diretório\n", dir_pai_str);
        return;
    }

    // Verificar se o diretório já existe
    if (procurar_entrada_no_diretorio(fd, sb, gdt, inode_pai_num, nome_dir_novo) != 0) {
        printf("mkdir: não foi possível criar o diretório '%s': Arquivo já existe\n", caminho_arg);
        return;
    }

    // Alocar recursos para o novo diretório (inode E um bloco de dados)
    uint32_t novo_dir_inode_num = alocar_inode(fd, sb, gdt);
    if (novo_dir_inode_num == 0) {
        printf("mkdir: falha ao alocar inode para novo diretório\n");
        return;
    }
    uint32_t novo_dir_bloco_num = alocar_bloco(fd, sb, gdt, novo_dir_inode_num);
    if (novo_dir_bloco_num == 0) {
        printf("mkdir: falha ao alocar bloco de dados para novo diretório\n");
        liberar_inode(fd, sb, gdt, novo_dir_inode_num); // Rollback
        return;
    }

    // Preparar o bloco de dados inicial com as entradas '.' e '..'
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    char* buffer_novo_bloco = malloc(tamanho_bloco);
    memset(buffer_novo_bloco, 0, tamanho_bloco);

    // Entrada '.' (aponta para si mesmo)
    ext2_dir_entry* entry_dot = (ext2_dir_entry*)buffer_novo_bloco;
    entry_dot->inode = novo_dir_inode_num;
    entry_dot->name_len = 1;
    entry_dot->file_type = EXT2_FT_DIR;
    memcpy(entry_dot->name, ".", 1);
    entry_dot->rec_len = 12;

    // Entrada '..' (aponta para o pai)
    ext2_dir_entry* entry_dotdot = (ext2_dir_entry*)(buffer_novo_bloco + entry_dot->rec_len);
    entry_dotdot->inode = inode_pai_num;
    entry_dotdot->name_len = 2;
    entry_dotdot->file_type = EXT2_FT_DIR;
    memcpy(entry_dotdot->name, "..", 2); 
    entry_dotdot->rec_len = tamanho_bloco - entry_dot->rec_len;

    escrever_bloco(fd, sb, novo_dir_bloco_num, buffer_novo_bloco);
    free(buffer_novo_bloco);

    // Inicializar e escrever o inode do novo diretório
    inode novo_dir_ino;
    memset(&novo_dir_ino, 0, sizeof(inode));
    novo_dir_ino.mode = EXT2_S_IFDIR | 0755; // Diretório, rwxr-xr-x
    novo_dir_ino.size = tamanho_bloco;
    novo_dir_ino.links_count = 2; // Começa com 2 links: '.' e a entrada no diretório pai
    novo_dir_ino.blocks = tamanho_bloco / 512;
    novo_dir_ino.block[0] = novo_dir_bloco_num;
    novo_dir_ino.atime = novo_dir_ino.mtime = novo_dir_ino.ctime = time(NULL);
    escrever_inode(fd, sb, gdt, novo_dir_inode_num, &novo_dir_ino);

    // Adicionar a entrada para o novo diretório no diretório pai
    if (adicionar_entrada_diretorio(fd, sb, gdt, &inode_pai, inode_pai_num, novo_dir_inode_num, nome_dir_novo, EXT2_FT_DIR) != 0) {
        printf("mkdir: falha ao adicionar entrada no diretório pai. Desfazendo operações...\n");
        liberar_bloco(fd, sb, gdt, novo_dir_bloco_num);
        liberar_inode(fd, sb, gdt, novo_dir_inode_num);
        return;
    }
    
    // Atualizar o inode do diretório pai
    inode_pai.links_count++; // A entrada '..' do novo diretório cria um novo link para o pai
    inode_pai.mtime = time(NULL);
    escrever_inode(fd, sb, gdt, inode_pai_num, &inode_pai);

    printf("Diretório '%s' criado com sucesso.\n", caminho_arg);
}


/**
 * @brief Executa a lógica do comando 'rmdir', removendo um diretório existente.
 */
void comando_rmdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("rmdir: faltando operando\n");
        return;
    }
    if (strcmp(caminho_arg, ".") == 0 || strcmp(caminho_arg, "..") == 0 || strcmp(caminho_arg, "/") == 0) {
        printf("rmdir: não é possível remover '%s': Diretório inválido ou protegido\n", caminho_arg);
        return;
    }

    // Encontra o inode do alvo e do seu pai
    uint32_t inode_alvo_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, caminho_arg);
    if (inode_alvo_num == 0) {
        printf("rmdir: não foi possível remover '%s': Diretório não encontrado\n", caminho_arg);
        return;
    }
    inode inode_alvo;
    if (ler_inode(fd, sb, gdt, inode_alvo_num, &inode_alvo) != 0) return;
    if (!EXT2_IS_DIR(inode_alvo.mode)) {
        printf("rmdir: não foi possível remover '%s': Não é um diretório\n", caminho_arg);
        return;
    }
    
    char copia_caminho[1024];
    strncpy(copia_caminho, caminho_arg, 1024);
    char* nome_dir_removido = basename(copia_caminho);
    strncpy(copia_caminho, caminho_arg, 1024);
    char* dir_pai_str = dirname(copia_caminho);
    uint32_t inode_pai_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, dir_pai_str);
    inode inode_pai;
    if (ler_inode(fd, sb, gdt, inode_pai_num, &inode_pai) != 0) return;

    // Verifica se o diretório está vazio
    if (diretorio_esta_vazio(fd, sb, &inode_alvo) != 1) {
        printf("rmdir: não foi possível remover '%s': Diretório não está vazio\n", caminho_arg);
        return;
    }

    // Remove a entrada do diretório pai
    if (remover_entrada_diretorio(fd, sb, &inode_pai, nome_dir_removido) != 0) {
        printf("rmdir: erro ao remover entrada do diretório pai.\n");
        return;
    }

    // Libera os recursos do diretório removido
    liberar_bloco(fd, sb, gdt, inode_alvo.block[0]); // Diretórios vazios só têm 1 bloco
    inode_alvo.dtime = time(NULL);
    inode_alvo.links_count = 0; // Diretório vazio não tem mais links
    escrever_inode(fd, sb, gdt, inode_alvo_num, &inode_alvo); // Salva o dtime e links_count
    liberar_inode(fd, sb, gdt, inode_alvo_num);

    // Atualiza o inode pai
    inode_pai.links_count--; // A entrada '..' que existia no dir removido foi-se embora
    inode_pai.mtime = time(NULL);
    escrever_inode(fd, sb, gdt, inode_pai_num, &inode_pai);

    printf("Diretório '%s' removido com sucesso.\n", caminho_arg);
}


/**
 * @brief (Função Auxiliar Estática) Procura e renomeia uma entrada em um único bloco de diretório.
 * @return 1 se renomeado com sucesso, 0 se não encontrado, -1 em erro (ex: nome novo muito longo).
 */
static int renomear_entrada_em_bloco(int fd, const superbloco* sb, uint32_t num_bloco, const char* nome_antigo, const char* nome_novo) {
    if (num_bloco == 0) return 0; // Bloco não alocado, não é um erro.
    
    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    char* buffer = malloc(tamanho_bloco);
    if (!buffer) {
        perror("rename (helper): falha ao alocar buffer");
        return -1;
    }

    if (ler_bloco(fd, sb, num_bloco, buffer) != 0) {
        free(buffer);
        return 0;
    }

    uint32_t offset = 0;
    size_t tam_nome_antigo = strlen(nome_antigo);
    while (offset < tamanho_bloco) {
        ext2_dir_entry* entry = (ext2_dir_entry*)(buffer + offset);
        if (entry->rec_len == 0) break;

        if (entry->inode != 0 && entry->name_len == tam_nome_antigo && strncmp(entry->name, nome_antigo, tam_nome_antigo) == 0) {
            
            // Calcula o espaço mínimo necessário para a nova entrada, de forma correta e segura.
            uint16_t rec_len_necessario_novo = (offsetof(ext2_dir_entry, name) + strlen(nome_novo) + 3) & ~3;

            if (rec_len_necessario_novo > entry->rec_len) {
                printf("rename: falha ao renomear. O novo nome é muito longo para o espaço disponível nesta entrada.\n");
                free(buffer);
                return -1; // Erro, para toda a operação.
            }

            // Se couber, faz a renomeação
            entry->name_len = strlen(nome_novo);
            memcpy(entry->name, nome_novo, entry->name_len);
            
            // Limpa o resto do campo de nome para não deixar lixo do nome antigo
            uint16_t tam_real_novo_sem_padding = offsetof(ext2_dir_entry, name) + entry->name_len;
            if (entry->rec_len > tam_real_novo_sem_padding) {
               memset((char*)entry + tam_real_novo_sem_padding, 0, entry->rec_len - tam_real_novo_sem_padding);
            }

            escrever_bloco(fd, sb, num_bloco, buffer);
            free(buffer);
            return 1; // Sucesso!
        }
        if (offset + entry->rec_len >= tamanho_bloco) break;
        offset += entry->rec_len;
    }
    
    free(buffer);
    return 0; // Não encontrado neste bloco
}



/**
 * @brief Executa a lógica do comando 'rename', renomeando um arquivo no diretório atual.
 * Esta  procura a entrada em blocos diretos e indiretos (simples e duplos).
 */
void comando_rename(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* nome_antigo_arg, char* nome_novo_arg) {
    // Validação dos argumentos
    if (nome_antigo_arg == NULL || nome_novo_arg == NULL) {
        printf("Uso: rename <nome_antigo> <nome_novo>\n");
        return;
    }
    if (strlen(nome_novo_arg) > EXT2_NAME_LEN) {
        printf("rename: novo nome do arquivo é muito longo\n");
        return;
    }
    if (strchr(nome_novo_arg, '/') != NULL) {
        printf("rename: novo nome não pode conter '/'. A renomeação é apenas no diretório atual.\n");
        return;
    }
    if (procurar_entrada_no_diretorio(fd, sb, gdt, inode_dir_atual, nome_novo_arg) != 0) {
        printf("rename: não foi possível renomear para '%s': Arquivo já existe\n", nome_novo_arg);
        return;
    }

    // Prepara buffers e variáveis para a busca
    inode dir_ino;
    if (ler_inode(fd, sb, gdt, inode_dir_atual, &dir_ino) != 0) return;

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
    uint32_t* buffer_ponteiros = malloc(tamanho_bloco);

    if (!buffer_ponteiros) {
        perror("rename: falha ao alocar buffer de ponteiros");
        return;
    }
    
    int status_busca = 0;

    // Itera sobre os blocos para encontrar e modificar a entrada
    
    // Busca em Blocos Diretos
    for (int i = 0; i < 12; ++i) {
        status_busca = renomear_entrada_em_bloco(fd, sb, dir_ino.block[i], nome_antigo_arg, nome_novo_arg);
        if (status_busca != 0) goto end_rename; // Se encontrou (1) ou deu erro (-1), termina.
    }

    // Busca em Bloco de Indireção Simples
    if (dir_ino.block[12] != 0) {
        if (ler_bloco(fd, sb, dir_ino.block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                status_busca = renomear_entrada_em_bloco(fd, sb, buffer_ponteiros[i], nome_antigo_arg, nome_novo_arg);
                if (status_busca != 0) goto end_rename;
            }
        }
    }
    
    // Busca em Bloco de Indireção Dupla
    if (dir_ino.block[13] != 0) {
        if (ler_bloco(fd, sb, dir_ino.block[13], buffer_ponteiros) == 0) { // Lê o bloco de ponteiros L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) continue; // Pula ponteiros nulos no bloco L1
                
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê o bloco de ponteiros L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        status_busca = renomear_entrada_em_bloco(fd, sb, bloco_L2[j], nome_antigo_arg, nome_novo_arg);
                        if (status_busca != 0) { // Se encontrou ou deu erro...
                            free(bloco_L2);     // ...libera a memória...
                            goto end_rename;   // ...e encerra a busca.
                        }
                    }
                }
                free(bloco_L2); // Libera o buffer L2 antes de ir para o próximo ponteiro L1
            }
        }
    }
    
    // A lógica para indireção tripla (block[14]) seguiria o mesmo padrão, com mais um nível de loop.

end_rename:
    // Finaliza a operação com base no resultado da busca
    if (status_busca == 1) { // Sucesso na renomeação
        // Atualiza o tempo de modificação do diretório pai
        dir_ino.mtime = time(NULL);
        escrever_inode(fd, sb, gdt, inode_dir_atual, &dir_ino);
        printf("Arquivo '%s' renomeado para '%s' com sucesso.\n", nome_antigo_arg, nome_novo_arg);
    } else if (status_busca == 0) { // Não encontrado após toda a busca
        printf("rename: não foi possível encontrar o arquivo '%s'\n", nome_antigo_arg);
    }
    // Se status_busca for -1, a mensagem de erro já foi impressa pelo helper.

    free(buffer_ponteiros);
}