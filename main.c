/**
 * @file main.c
 * @brief Ponto de entrada do shell interativo para o sistema de arquivos Ext2.
 *
 * Este programa inicializa a conexão com uma imagem de disco Ext2 e fornece
 * um shell simples para interagir com o sistema de arquivos através de comandos
 * como 'info', 'help' e 'exit'.
 *
 * Data de criação: 10 de junho de 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Para close()
#include <fcntl.h>  // Para open()

#include "headers.h"


void imprimir_ajuda(void) {
    printf("\n--- Shell Ext2 - Comandos Disponíveis ---\n");
    printf("  print superblock          - Exibe as informações detalhadas do superbloco.\n");
    printf("  print inode <numero>      - Exibe as informações detalhadas de um inode específico.\n");
    printf("  help                      - Mostra esta mensagem de ajuda.\n");
    printf("  exit | quit               - Encerra o programa.\n");
    printf("--------------------------------------------------------------------------\n\n");
}


/**
 * @brief Função principal que executa o shell Ext2.
 */
int main(int argc, char *argv[]) {
    // 1. VERIFICAÇÃO DOS ARGUMENTOS
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <caminho_para_a_imagem_ext2>\n", argv[0]);
        return 1; // Encerra com código de erro
    }
    const char* caminho_imagem = argv[1];

    // 2. INICIALIZAÇÃO DO SISTEMA DE ARQUIVOS
    printf("Abrindo a imagem do disco: %s\n", caminho_imagem);

    // Abre a imagem em modo de leitura (por enquanto)
    int fd = open(caminho_imagem, O_RDONLY);
    if (fd == -1) {
        perror("Erro fatal ao abrir a imagem do disco");
        return 1;
    }

    // Declara as estruturas principais que usaremos
    superbloco sb;
    group_desc *gdt = NULL;
    uint32_t num_grupos = 0;

    // Tenta ler o superbloco
    if (ler_superbloco(fd, &sb) != 0) {
        fprintf(stderr, "Erro fatal: não foi possível ler o superbloco.\n");
        close(fd);
        return 1;
    }

    // Valida o superbloco para garantir que é um FS Ext2
    if (!validar_superbloco(&sb)) {
        fprintf(stderr, "Erro fatal: A imagem não parece ser um sistema de arquivos Ext2 válido.\n");
        close(fd);
        return 1;
    }

    printf("Superbloco lido e validado com sucesso!\n");

    // Lê a tabela de descritores de grupo (GDT)
    gdt = ler_descritores_grupo(fd, &sb, &num_grupos);
    if (gdt == NULL) {
        fprintf(stderr, "Erro fatal: não foi possível ler a tabela de descritores de grupo.\n");
        close(fd);
        return 1;
    }
    printf("Tabela de descritores de grupo lida com sucesso (%u grupos).\n\n", num_grupos);

    // 3. LOOP PRINCIPAL DO SHELL
    char linha_comando[256];
    char prompt[50]; // Buffer para o prompt



    do {
        snprintf(prompt, sizeof(prompt), "[/]> ");
        printf("%s", prompt);

        if (fgets(linha_comando, sizeof(linha_comando), stdin) == NULL) {
            printf("\nSaindo (EOF detectado)...\n");
            break; 
        }

        char* comando = strtok(linha_comando, " \t\n\r");
        if (comando == NULL) {
            continue;
        }

        if (strcmp(comando, "print") == 0) {
            char* arg1 = strtok(NULL, " \t\n\r");

            if (arg1 != NULL && strcmp(arg1, "superblock") == 0) {
                if (strtok(NULL, " \t\n\r") == NULL) {
                    print_superbloco(&sb);
                } else {
                    printf("Comando 'print superblock' não aceita argumentos adicionais.\n");
                }
            } 
            else if (arg1 != NULL && strcmp(arg1, "inode") == 0) {
                char* arg2 = strtok(NULL, " \t\n\r");
                if (arg2 == NULL) {
                    printf("Uso: print inode <numero>\n");
                } else if (strtok(NULL, " \t\n\r") != NULL) {
                    printf("Comando 'print inode' recebeu argumentos demais.\n");
                } else {
                    char* endptr;
                    long num_inode_long = strtol(arg2, &endptr, 10);
                    if (*endptr != '\0') {
                        printf("Erro: O número do inode '%s' é inválido.\n", arg2);
                    } else {
                        uint32_t num_inode = (uint32_t)num_inode_long;
                        inode ino_para_imprimir;
                        if (ler_inode(fd, &sb, gdt, num_inode, &ino_para_imprimir) == 0) {
                            print_inode(&ino_para_imprimir, num_inode);
                        }
                    }
                }
            }
            else if (arg1 != NULL && strcmp(arg1, "groups") == 0) {
                if (strtok(NULL, " \t\n\r") == NULL) {
                    print_groups(gdt, num_grupos);
                } else {
                    printf("Comando 'print groups' não aceita argumentos adicionais.\n");
                }
            }
            else {
                printf("Comando 'print' inválido. Uso: 'print superblock', 'print inode <numero>', ou 'print groups'\n");
            }
        } 
        else if (strcmp(comando, "help") == 0) {
            imprimir_ajuda();
        } 
        else if (strcmp(comando, "exit") == 0 || strcmp(comando, "quit") == 0) {
            printf("Saindo...\n");
            break;
        } 
        else {
            printf("Comando desconhecido: '%s'. Digite 'help' para ver a lista de comandos.\n", comando);
        }

    } while (1); // Loop infinito, quebra com 'exit', 'quit' ou Ctrl+D

    // 4. LIMPEZA E ENCERRAMENTO
    printf("Liberando recursos e fechando o disco.\n");
    liberar_descritores_grupo(gdt); // Libera a memória alocada para a GDT
    close(fd); // Fecha o arquivo da imagem

    return 0; // Sucesso
}