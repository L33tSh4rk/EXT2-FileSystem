/**
 * @file       commands.c
 * @brief      Implementação da lógica de cada comando de usuário do shell (ls, cd, cat, etc.).
 * 
 * @author      Allan Custódio Diniz Marques (L33tSh4rk) 
 * @author      Vitor Hugo Melo Ribeiro
 *
 * Cada função neste arquivo corresponde a um comando que o usuário pode digitar
 * e orquestra as chamadas para as funções de baixo nível em systemOp.c.
 *
 * Data de criação: 24 de abril de 2025
 * Data de atualização: 6 de julho de 2025
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h> // Para a função dirname() e basename()
#include <time.h>
#include <stdbool.h>
#include <stddef.h>

#include "commands.h" // Inclui protótipos
#include "headers.h"  // Inclui definições e funções de baixo nível



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

/**
 * @brief Executa a lógica do comando 'info', que mostra os atributos da imagem.
 */
void comando_info(const superbloco* sb, uint32_t num_grupos, char *argumentos) {
    if (argumentos != NULL) {
        printf("Comando 'info' não aceita argumentos.\n");
        return;
    }
    imprimir_formato_info(sb, num_grupos);
}


/**
 * @brief Executa a lógica do comando 'attr', que mostra as permissões de um dado arquivo ou diretório.
 */
void comando_attr(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("Uso: attr <caminho>\n");
        return;
    }
    uint32_t inode_ponto_partida = (argumentos[0] == '/') ? EXT2_ROOT_INO : inode_dir_atual;
    uint32_t inode_num = caminho_para_inode(fd, sb, gdt, inode_ponto_partida, argumentos);
    if (inode_num == 0) {
        printf("Erro: Arquivo ou diretório não encontrado: '%s'\n", argumentos);
    } else {
        inode ino;
        if (ler_inode(fd, sb, gdt, inode_num, &ino) == 0) {
            imprimir_formato_attr(&ino);
        } else {
            fprintf(stderr, "Erro crítico ao ler o inode %u.\n", inode_num);
        }
    }
}

/**
 * @brief Executa a lógica do comando 'cat', que é responsável por mostrar o conteúdo de um arquivo regular em formato de texto.
 */
void comando_cat(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("Uso: cat <caminho_para_arquivo>\n");
        return;
    }
    uint32_t inode_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, argumentos);
    if (inode_num == 0) {
        printf("cat: %s: Arquivo não encontrado\n", argumentos);
        return;
    }
    inode ino;
    if (ler_inode(fd, sb, gdt, inode_num, &ino) != 0) return;
    if (EXT2_IS_DIR(ino.mode)) {
        // Se for um diretório, imprime o erro específico e para.
        printf("cat: %s: É um diretório\n", argumentos);
        return;
    }
    
    if (!EXT2_IS_REG(ino.mode)) {
        // Se não for um diretório, mas também não for um arquivo regular (ex: link simbólico, socket),
        // imprime um erro genérico.
        printf("cat: %s: Não é possível ler o conteúdo deste tipo de arquivo\n", argumentos);
        return;
    }
    
    char* conteudo = ler_conteudo_arquivo(fd, sb, &ino);
    if (conteudo) {
        printf("%s", conteudo);
        free(conteudo);
    }
}


/**
 * @brief Executa a lógica do comando 'ls', responsável por listar os arquivos e diretórios presentes no diretório corrente.
 */
void comando_ls(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    uint32_t inode_a_listar;
    char* nome_alvo;

    // Determina qual diretório/arquivo listar com base nos argumentos
    if (argumentos == NULL) {
        // Nenhum argumento: lista o diretório de trabalho atual
        inode_a_listar = inode_dir_atual;
        nome_alvo = "."; // Usado para mensagens de erro
    } else {
        // Argumento fornecido: resolve o caminho
        uint32_t ponto_partida = (argumentos[0] == '/') ? EXT2_ROOT_INO : inode_dir_atual;
        inode_a_listar = caminho_para_inode(fd, sb, gdt, ponto_partida, argumentos);
        nome_alvo = argumentos;
    }

    if (inode_a_listar == 0) {
        printf("ls: não foi possível acessar '%s': Arquivo ou diretório não encontrado\n", nome_alvo);
        return;
    }

    // Lê o inode do alvo
    inode ino;
    if (ler_inode(fd, sb, gdt, inode_a_listar, &ino) != 0) {
        return;
    }

    // Se for um arquivo regular, apenas imprime seu nome e termina
    if (!EXT2_IS_DIR(ino.mode)) {
        printf("%s\n", nome_alvo);
        return;
    }

    // Se for um diretório, prepara para listar seu conteúdo
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

    // --- Itera sobre os 12 ponteiros de blocos diretos ---
    for (int i = 0; i < 12; ++i) {
        if (ino.block[i] == 0) break;
        if (ler_bloco(fd, sb, ino.block[i], buffer_dados) == 0) {
            imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
        }
    }

    // --- Processa o bloco de indireção simples (block[12]) ---
    if (ino.block[12] != 0) {
        if (ler_bloco(fd, sb, ino.block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) break;
                if (ler_bloco(fd, sb, buffer_ponteiros[i], buffer_dados) == 0) {
                    imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
                }
            }
        }
    }

    // --- Processa o bloco de indireção dupla (block[13]) ---
    if (ino.block[13] != 0) {
        if (ler_bloco(fd, sb, ino.block[13], buffer_ponteiros) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) break;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        if (bloco_L2[j] == 0) break;
                        if (ler_bloco(fd, sb, bloco_L2[j], buffer_dados) == 0) {
                            imprimir_entradas_de_bloco_dir(buffer_dados, tamanho_bloco);
                        }
                    }
                }
                free(bloco_L2);
            }
        }
    }
    
    // Logica para indireção tripla omitida por questoes de simplicidade

    free(buffer_dados);
    free(buffer_ponteiros);
}

/**
 * @brief Executa a lógica do comando 'pwd', que imprime o caminho absoluto até o diretório atual.
 */
void comando_pwd(const char* diretorio_atual_str, char* argumentos) {
    // Validação para garantir que o comando não recebeu argumentos extras
    if (argumentos != NULL) {
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
                char* argumentos) {
    if (argumentos == NULL) {
        // 'cd' sem argumentos não faz nada
        return;
    }

    // Encontra o inode do diretório de destino
    uint32_t inode_destino = caminho_para_inode(fd, sb, gdt, *p_inode_dir_atual, argumentos);

    if (inode_destino == 0) {
        printf("cd: %s: Arquivo ou diretório não encontrado\n", argumentos);
        return;
    }

    // Verifica se o destino é realmente um diretório
    inode ino;
    if (ler_inode(fd, sb, gdt, inode_destino, &ino) != 0) {
        return; // Erro já foi impresso por ler_inode
    }
    if (!EXT2_IS_DIR(ino.mode)) {
        printf("cd: %s: Não é um diretório\n", argumentos);
        return;
    }

    // Atualiza o estado do shell (o inode e a string do caminho)
    *p_inode_dir_atual = inode_destino;

    // Lógica para atualizar a string do caminho
    if (strcmp(argumentos, "..") == 0) {
        // Sobe um nível. Usa dirname para encontrar o diretório pai da string atual.
        // Precisa de uma cópia, pois dirname pode modificar a string.
        char temp_path[1024];
        strncpy(temp_path, diretorio_atual_str, 1024);
        char* parent = dirname(temp_path);
        strcpy(diretorio_atual_str, parent);
    } else if (strcmp(argumentos, ".") != 0) {
        // Se o caminho for absoluto, apenas copie-o
        if (argumentos[0] == '/') {
            strcpy(diretorio_atual_str, argumentos);
        } else {
            // Se for relativo, anexe-o ao caminho atual
            // Adiciona a barra, a menos que já estejamos na raiz
            if (strcmp(diretorio_atual_str, "/") != 0) {
                strcat(diretorio_atual_str, "/");
            }
            strcat(diretorio_atual_str, argumentos);
        }
    }
    // Se for 'cd .', não faz nada com a string.

    // Garante que a raiz seja sempre apenas "/"
    if (strlen(diretorio_atual_str) > 1 && diretorio_atual_str[strlen(diretorio_atual_str) - 1] == '/') {
        diretorio_atual_str[strlen(diretorio_atual_str) - 1] = '\0';
    }
}

/**
 * @brief Executa a lógica do comando 'touch', criando um arquivo vazio.
 */
void comando_touch(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("touch: faltando operando de arquivo\n");
        return;
    }
    
    char copia_caminho1[1024], copia_caminho2[1024];
    strncpy(copia_caminho1, argumentos, 1024);
    strncpy(copia_caminho2, argumentos, 1024);
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
        printf("touch: não foi possível criar o arquivo '%s': Arquivo já existe\n", argumentos);
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

    printf("Arquivo '%s' criado com sucesso.\n", argumentos);
}



/**
 * @brief Executa a lógica do comando 'rm', removendo um arquivo regular.
 */
void comando_rm(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("rm: faltando operando\n");
        return;
    }

    uint32_t inode_alvo_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, argumentos);
    if (inode_alvo_num == 0) {
        printf("rm: não foi possível remover '%s': Arquivo não encontrado\n", argumentos);
        return;
    }

    inode inode_alvo;
    if (ler_inode(fd, sb, gdt, inode_alvo_num, &inode_alvo) != 0) return;
    if (EXT2_IS_DIR(inode_alvo.mode)) {
        printf("rm: não foi possível remover '%s': É um diretório\n", argumentos);
        return;
    }
    
    char copia_caminho[1024];
    strncpy(copia_caminho, argumentos, 1024);
    char* nome_arquivo = basename(copia_caminho);
    strncpy(copia_caminho, argumentos, 1024);
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

    printf("Arquivo '%s' removido com sucesso.\n", argumentos);
}



/**
 * @brief Executa a lógica do comando 'mkdir', criando um novo diretório vazio.
 */
void comando_mkdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("mkdir: faltando operando\n");
        return;
    }

    // Separar caminho pai e nome do novo diretório
    char copia_caminho1[1024], copia_caminho2[1024];
    strncpy(copia_caminho1, argumentos, 1024);
    strncpy(copia_caminho2, argumentos, 1024);
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
        printf("mkdir: não foi possível criar o diretório '%s': Arquivo já existe\n", argumentos);
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

    printf("Diretório '%s' criado com sucesso.\n", argumentos);
}


/**
 * @brief Executa a lógica do comando 'rmdir', removendo um diretório existente.
 */
void comando_rmdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("rmdir: faltando operando\n");
        return;
    }
    if (strcmp(argumentos, ".") == 0 || strcmp(argumentos, "..") == 0 || strcmp(argumentos, "/") == 0) {
        printf("rmdir: não é possível remover '%s': Diretório inválido ou protegido\n", argumentos);
        return;
    }

    // Encontra o inode do alvo e do seu pai
    uint32_t inode_alvo_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, argumentos);
    if (inode_alvo_num == 0) {
        printf("rmdir: não foi possível remover '%s': Diretório não encontrado\n", argumentos);
        return;
    }
    inode inode_alvo;
    if (ler_inode(fd, sb, gdt, inode_alvo_num, &inode_alvo) != 0) return;
    if (!EXT2_IS_DIR(inode_alvo.mode)) {
        printf("rmdir: não foi possível remover '%s': Não é um diretório\n", argumentos);
        return;
    }
    
    char copia_caminho[1024];
    strncpy(copia_caminho, argumentos, 1024);
    char* nome_dir_removido = basename(copia_caminho);
    strncpy(copia_caminho, argumentos, 1024);
    char* dir_pai_str = dirname(copia_caminho);
    uint32_t inode_pai_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, dir_pai_str);
    inode inode_pai;
    if (ler_inode(fd, sb, gdt, inode_pai_num, &inode_pai) != 0) return;

    // Verifica se o diretório está vazio
    if (diretorio_esta_vazio(fd, sb, &inode_alvo) != 1) {
        printf("rmdir: não foi possível remover '%s': Diretório não está vazio\n", argumentos);
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

    printf("Diretório '%s' removido com sucesso.\n", argumentos);
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
 * @brief Executa a lógica do comando 'rename', renomeando um arquivo ou diretório.
 * Esta versão final possui um parser que lida com espaços e busca em blocos
 * diretos e indiretos (simples e duplos).
 */
void comando_rename(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
    if (argumentos == NULL) {
        printf("Uso: rename <nome_antigo> <nome_novo>\n");
        return;
    }

    // parser que lida com espaços, mais complexo que o atual na função main para tratar de arquivos com nomes espaçados
    // tenta encontrar o primeiro nome de diretório, se encontrar, o nome a frente será a renomeação, caso contrário concatena o próx. 
    // nome e procura pelo nome do arquivo --> se passar por todas as palavras e não achar o nome do diretorio, retorna erro
    char nome_antigo_candidato[EXT2_NAME_LEN + 1] = {0};
    char nome_antigo_final[EXT2_NAME_LEN + 1] = {0};
    char* nome_novo_final = NULL;
    
    char copia_argumentos[1024];
    strncpy(copia_argumentos, argumentos, sizeof(copia_argumentos) - 1);
    copia_argumentos[sizeof(copia_argumentos) - 1] = '\0';

    char* token_atual = strtok(copia_argumentos, " \t");
    
    while (token_atual != NULL) {
        if (strlen(nome_antigo_candidato) > 0) {
            strcat(nome_antigo_candidato, " ");
        }
        strcat(nome_antigo_candidato, token_atual);
        
        if (procurar_entrada_no_diretorio(fd, sb, gdt, inode_dir_atual, nome_antigo_candidato) != 0) {
            strcpy(nome_antigo_final, nome_antigo_candidato);
            size_t len_encontrado = strlen(nome_antigo_final);
            nome_novo_final = argumentos + len_encontrado;
        }
        token_atual = strtok(NULL, " \t");
    }

    if (nome_novo_final) while(*nome_novo_final == ' ' || *nome_novo_final == '\t') nome_novo_final++;

    // fim do parser

    // validação dos argumentos que o parser encontrou
    if (strlen(nome_antigo_final) == 0 || nome_novo_final == NULL || strlen(nome_novo_final) == 0) {
        printf("rename: não foi possível encontrar o arquivo de origem ou o novo nome não foi fornecido.\n");
        return;
    }
    if (strlen(nome_novo_final) > EXT2_NAME_LEN) {
        printf("rename: novo nome do arquivo é muito longo\n");
        return;
    }
    if (strchr(nome_novo_final, '/') != NULL) {
        printf("rename: novo nome não pode conter '/'.\n");
        return;
    }
    if (procurar_entrada_no_diretorio(fd, sb, gdt, inode_dir_atual, nome_novo_final) != 0) {
        printf("rename: não foi possível renomear para '%s': Arquivo já existe\n", nome_novo_final);
        return;
    }

    
    // prepara para a busca no disco
    inode dir_ino;
    if (ler_inode(fd, sb, gdt, inode_dir_atual, &dir_ino) != 0) return;

    uint32_t tamanho_bloco = calcular_tamanho_do_bloco(sb);
    uint32_t ponteiros_por_bloco = tamanho_bloco / sizeof(uint32_t);
    uint32_t* buffer_ponteiros = malloc(tamanho_bloco);
    if (!buffer_ponteiros) { perror("rename: falha ao alocar buffer"); return; }
    
    int status_busca = 0;

    // busca em Blocos Diretos
    for (int i = 0; i < 12; ++i) {
        status_busca = renomear_entrada_em_bloco(fd, sb, dir_ino.block[i], nome_antigo_final, nome_novo_final);
        if (status_busca != 0) goto end_rename;
    }

    // busca em Bloco de Indireção Simples
    if (dir_ino.block[12] != 0) {
        if (ler_bloco(fd, sb, dir_ino.block[12], buffer_ponteiros) == 0) {
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                status_busca = renomear_entrada_em_bloco(fd, sb, buffer_ponteiros[i], nome_antigo_final, nome_novo_final);
                if (status_busca != 0) goto end_rename;
            }
        }
    }
    
    // busca em Bloco de Indireção Dupla
    if (dir_ino.block[13] != 0) {
        if (ler_bloco(fd, sb, dir_ino.block[13], buffer_ponteiros) == 0) { // Lê L1
            for (uint32_t i = 0; i < ponteiros_por_bloco; ++i) {
                if (buffer_ponteiros[i] == 0) continue;
                uint32_t* bloco_L2 = malloc(tamanho_bloco);
                if (bloco_L2 && ler_bloco(fd, sb, buffer_ponteiros[i], bloco_L2) == 0) { // Lê L2
                    for (uint32_t j = 0; j < ponteiros_por_bloco; ++j) {
                        status_busca = renomear_entrada_em_bloco(fd, sb, bloco_L2[j], nome_antigo_final, nome_novo_final);
                        if (status_busca != 0) { free(bloco_L2); goto end_rename; }
                    }
                }
                free(bloco_L2);
            }
        }
    }
    
end_rename:
    // finaliza a operação com base no resultado da busca
    if (status_busca == 1) {
        dir_ino.mtime = time(NULL);
        escrever_inode(fd, sb, gdt, inode_dir_atual, &dir_ino);
        uint32_t inode_renomeado_num = procurar_entrada_no_diretorio(fd, sb, gdt, inode_dir_atual, nome_novo_final);
        if (inode_renomeado_num != 0) {
            inode inode_renomeado;
            if (ler_inode(fd, sb, gdt, inode_renomeado_num, &inode_renomeado) == 0) {
                inode_renomeado.ctime = time(NULL);
                escrever_inode(fd, sb, gdt, inode_renomeado_num, &inode_renomeado);
            }
        }
        printf("'%s' renomeado para '%s' com sucesso.\n", nome_antigo_final, nome_novo_final);
    } else if (status_busca == 0) {
        printf("rename: não foi possível encontrar o arquivo '%s'\n", nome_antigo_final);
    }
    
    free(buffer_ponteiros);
}



/**
 * @brief Executa a lógica do comando 'cp', que copia um arquivo de DENTRO da imagem Ext2
 * para o sistema de arquivos local (host).
 */
void comando_cp(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos) {
   
    // analisa argumentos para obter origem (na imagem) e destino (no host)
    char* caminho_origem_ext2 = strtok(argumentos, " \t");
    char* caminho_destino_host = strtok(NULL, " \t\n\r");

    if (caminho_origem_ext2 == NULL || caminho_destino_host == NULL) {
        printf("Uso: cp <arquivo_na_imagem> <caminho_local_de_destino>\n");
        return;
    }

    // primeira fase - lê arquivo de dentro da imagem para a memória

    // encontra e valida o arquivo de origem na imagem
    uint32_t inode_origem_num = caminho_para_inode(fd, sb, gdt, inode_dir_atual, caminho_origem_ext2);
    if (inode_origem_num == 0) {
        printf("cp: arquivo de origem '%s' não encontrado na imagem.\n", caminho_origem_ext2);
        return;
    }

    inode ino_origem;
    if (ler_inode(fd, sb, gdt, inode_origem_num, &ino_origem) != 0 || !EXT2_IS_REG(ino_origem.mode)) {
        printf("cp: '%s' não é um arquivo regular.\n", caminho_origem_ext2);
        return;
    }
    
    if (ino_origem.size == 0) {
        printf("Aviso: arquivo de origem '%s' está vazio.\n", caminho_origem_ext2);
    }

    // lê o conteúdo completo do arquivo para a memória RAM
    char* buffer_conteudo = ler_conteudo_arquivo(fd, sb, &ino_origem);
    if (buffer_conteudo == NULL) {
        fprintf(stderr, "cp: falha ao ler o conteúdo do arquivo de origem.\n");
        return;
    }

    // segunda fase - escreve conteudo da memoria para o disco local

    // abre o arquivo de destino no seu computador para escrita binária ("wb")
    FILE* arquivo_destino = fopen(caminho_destino_host, "wb");
    if (arquivo_destino == NULL) {
        perror("cp: falha ao criar o arquivo de destino no seu computador");
        free(buffer_conteudo); // libera a memoria
        return;
    }

    // escreve o conteúdo do buffer no novo arquivo
    size_t bytes_escritos = fwrite(buffer_conteudo, 1, ino_origem.size, arquivo_destino);

    // fecha o arquivo de destino e libera a memória
    fclose(arquivo_destino);
    free(buffer_conteudo);

    if (bytes_escritos != ino_origem.size) {
        fprintf(stderr, "cp: erro de escrita. O arquivo de destino pode estar incompleto.\n");
    } else {
        printf("Arquivo '%s' copiado para '%s' com sucesso (%u bytes).\n", caminho_origem_ext2, caminho_destino_host, ino_origem.size);
    }
}