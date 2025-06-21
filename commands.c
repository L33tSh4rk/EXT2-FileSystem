#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h> // Para a função dirname() e basename()
#include <time.h>

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


void comando_attr(int fd, const superbloco* sb, const group_desc* gdt, char* caminho_arg) {
    if (caminho_arg == NULL) {
        printf("Uso: attr <caminho_para_arquivo_ou_diretorio>\n");
        return;
    }
    
    if (strtok(NULL, " \t\n\r") != NULL) {
        printf("Comando 'attr' recebeu argumentos demais.\n");
        return;
    }

    uint32_t inode_num = caminho_para_inode(fd, sb, gdt, EXT2_ROOT_INO, caminho_arg);
    
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

    // --- LÓGICA PARA DIRETÓRIOS ---
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

    // 1. Itera sobre os 12 ponteiros de blocos diretos
    for (int i = 0; i < 12; ++i) {
        uint32_t num_bloco = ino.block[i];
        if (num_bloco == 0) break;

        if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
            imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
        }
    }

    // 2. Processa o bloco de indireção simples (block[12])
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

    // 3. Processa o bloco de indireção dupla (block[13])
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

    // 4. Processa o bloco de indireção tripla (block[14])
    if (ino.block[14] != 0) {
        if (ler_bloco(fd, sb, ino.block[14], buffer_ponteiros) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) break;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        if (bloco_L2[j] == 0) break;
                        uint32_t* bloco_L3 = malloc(tamanho_bloco);
                        if (bloco_L3 && ler_bloco(fd, sb, bloco_L2[j], bloco_L3) == 0) { // Lê L3
                            for (uint32_t k = 0; k < ponteiros_por_bloco; ++k) {
                                uint32_t num_bloco = bloco_L3[k];
                                if (num_bloco == 0) break;
                                if (ler_bloco(fd, sb, num_bloco, buffer_dados) == 0) {
                                    imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
                                }
                            }
                        }
                        free(bloco_L3);
                    }
                }
                free(bloco_L2);
            }
        }
    }

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
        // 'cd' sem argumentos não faz nada, ou poderia ir para um diretório "home" se tivéssemos um.
        return;
    }

    // 1. Encontra o inode do diretório de destino
    uint32_t inode_destino = caminho_para_inode(fd, sb, gdt, *p_inode_dir_atual, caminho_arg);

    if (inode_destino == 0) {
        printf("cd: %s: Arquivo ou diretório não encontrado\n", caminho_arg);
        return;
    }

    // 2. Verifica se o destino é realmente um diretório
    inode ino;
    if (ler_inode(fd, sb, gdt, inode_destino, &ino) != 0) {
        return; // Erro já foi impresso por ler_inode
    }
    if (!EXT2_IS_DIR(ino.mode)) {
        printf("cd: %s: Não é um diretório\n", caminho_arg);
        return;
    }

    // 3. Atualiza o estado do shell (o inode e a string do caminho)
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

    uint32_t inode_existente_num = procurar_entrada_no_diretorio(fd, sb, gdt, inode_pai_num, nome_arquivo_novo);
    if (inode_existente_num != 0) {
        inode inode_existente;
        if (ler_inode(fd, sb, gdt, inode_existente_num, &inode_existente) == 0) {
            inode_existente.atime = inode_existente.mtime = time(NULL);
            escrever_inode(fd, sb, gdt, inode_existente_num, &inode_existente);
        }
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