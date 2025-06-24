/**
 * @file main.c
 * @brief Ponto de entrada do shell interativo para o sistema de arquivos Ext2.
 *
 * Este programa inicializa a conexão com uma imagem de disco Ext2 e fornece
 * um shell simples para interagir com o sistema de arquivos através de comandos
 * como 'info', 'help' e 'exit'.
 *
 * Data de criação: 24 de abril de 2025
 * Data de atualização: 23 de junho de 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <fcntl.h>  

#include "headers.h"
#include "commands.h"


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
    int fd = open(caminho_imagem, O_RDWR);
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


    uint32_t diretorio_atual_inode = EXT2_ROOT_INO;

    // Define a string do caminho atual, começando na raiz.
    char diretorio_atual_str[1024] = "/";

    // 3. LOOP PRINCIPAL DO SHELL
    char linha_comando[256];
    char prompt[1024 + 4]; // Buffer para o prompt



    do {
        snprintf(prompt, sizeof(prompt), "[%s]> ", diretorio_atual_str);
        printf("\n%s", prompt);

        if (fgets(linha_comando, sizeof(linha_comando), stdin) == NULL) {
            printf("\nSaindo (EOF detectado)...\n");
            break; 
        }

        char* comando = strtok(linha_comando, " \t\n\r");
        if (comando == NULL) {
            continue;
        }

        if (strcmp(comando, "print") == 0) {
            char* subcomando = strtok(NULL, " \t\n\r");
            if (subcomando == NULL) {
                printf("Comando 'print' incompleto. Uso: 'print superblock', 'print inode <n>', 'print groups'.\n");
            
            } else if (strcmp(subcomando, "superblock") == 0) {
                comando_print_superblock(&sb);
            
            } else if (strcmp(subcomando, "inode") == 0) {
                char* arg_num_inode = strtok(NULL, " \t\n\r");
                comando_print_inode(fd, &sb, gdt, arg_num_inode);
            
            } else if (strcmp(subcomando, "groups") == 0) {
                comando_print_groups(gdt, num_grupos);
            
            } else {
                printf("Argumento desconhecido para 'print': '%s'\n", subcomando);
            }
        }
        else if (strcmp(comando, "info") == 0) {
            comando_info(&sb, num_grupos);
        }
        
        else if (strcmp(comando, "attr") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_attr(fd, &sb, gdt, caminho_arg);
        }
        
        else if (strcmp(comando, "cat") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_cat(fd, &sb, gdt, caminho_arg);
        }

         else if (strcmp(comando, "ls") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_ls(fd, &sb, gdt, diretorio_atual_inode, caminho_arg);
        }

        else if (strcmp(comando, "cd") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_cd(fd, &sb, gdt, &diretorio_atual_inode, diretorio_atual_str, caminho_arg);
        }

        else if (strcmp(comando, "pwd") == 0){
            comando_pwd(diretorio_atual_str);
        }

        else if (strcmp(comando, "touch") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_touch(fd, &sb, gdt, diretorio_atual_inode, caminho_arg);

        }

        else if (strcmp(comando, "rm") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_rm(fd, &sb, gdt, diretorio_atual_inode, caminho_arg);
        }

        else if (strcmp(comando, "mkdir") == 0) {
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_mkdir(fd, &sb, gdt, diretorio_atual_inode, caminho_arg);
        }

        else if (strcmp(comando, "rmdir") == 0){
            char* caminho_arg = strtok(NULL, " \t\n\r");
            comando_rmdir(fd, &sb, gdt, diretorio_atual_inode, caminho_arg);
        }

        else if (strcmp(comando, "rename") == 0){
            char* caminho_arg1 = strtok(NULL, " \t\n\r");
            char* caminho_arg2 = strtok(NULL, " \t\n\r");
            comando_rename(fd, &sb, gdt, diretorio_atual_inode, caminho_arg1, caminho_arg2);
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