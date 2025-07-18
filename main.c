/**
 * @file       main.c
 * @brief      Ponto de entrada do shell, responsável pelo loop principal e por despachar os comandos.
 * @author     Allan Custódio Diniz Marques (L33tSh4rk)
 *
 * Este arquivo inicializa a conexão com a imagem de disco, gerencia o estado do
 * shell (como o diretório atual) e contém o loop principal que lê a entrada do
 * usuário e chama a função de comando apropriada.
 *
 * Data de criação: 24 de maio de 2025
 * Data de atualização: 6 de julho de 2025
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <fcntl.h>  

#include "headers.h"
#include "commands.h"


void imprimir_ajuda(void) {
    printf("\n========================================== Shell Ext2 - Comandos Disponíveis ==========================================\n");

    printf("\n  --- Comandos de Navegação e Inspeção ---\n");
    printf("  %-45s - Lista o conteúdo do diretório atual ou do [caminho] especificado.\n", "ls [caminho]");
    printf("  %-45s - Muda para o diretório de trabalho especificado pelo <caminho>.\n", "cd <caminho>");
    printf("  %-45s - Mostra o caminho absoluto do diretório de trabalho atual.\n", "pwd");
    printf("  %-45s - Exibe o conteúdo de um arquivo de texto.\n", "cat <arquivo>");
    printf("  %-45s - Mostra os atributos formatados de um arquivo ou diretório.\n", "attr <arquivo|diretório>");
    printf("  %-45s - Mostra um resumo das informações do sistema de arquivos.\n", "info");

    printf("\n  --- Comandos de Criação e Modificação ---\n");
    printf("  %-45s - Cria um arquivo vazio ou atualiza seu timestamp.\n", "touch <arquivo>");
    printf("  %-45s - Cria um novo diretório.\n", "mkdir <diretório>");
    printf("  %-45s - Renomeia um arquivo ou diretório no diretório atual.\n", "rename <nome_antigo> <nome_novo>");
    printf("  %-45s - Copia um arquivo da imagem para o seu computador.\n", "cp <origem_na_imagem> <destino_local_absoluto>");
    
    printf("\n  --- Comandos de Remoção ---\n");
    printf("  %-45s - Remove (apaga) um arquivo.\n", "rm <arquivo>");
    printf("  %-45s - Remove um diretório vazio.\n", "rmdir <diretório>");

    printf("\n  --- Comandos de Depuração ---\n");
    printf("  %-45s - Exibe os dados brutos do superbloco.\n", "print superblock");
    printf("  %-45s - Exibe os dados brutos de um inode específico.\n", "print inode <numero>");
    printf("  %-45s - Exibe os dados brutos de todos os descritores de grupo.\n", "print groups");

    printf("\n  --- Comandos do Shell ---\n");
    printf("  %-45s - Mostra esta mensagem de ajuda.\n", "help");
    printf("  %-45s - Encerra o programa.\n", "exit | quit");

    printf("=======================================================================================================================\n\n");
}


/**
 * @brief Função principal que executa o shell Ext2.
 */
int main(int argc, char *argv[]) {
    // VERIFICAÇÃO DOS ARGUMENTOS
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <caminho_para_a_imagem_ext2>\n", argv[0]);
        return 1; // Encerra com código de erro
    }
    const char* caminho_imagem = argv[1];

    // INICIALIZAÇÃO DO SISTEMA DE ARQUIVOS
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

    // LOOP PRINCIPAL DO SHELL
    char linha_comando[256];
    char prompt[1024 + 4]; // Buffer para o prompt



    do {
        snprintf(prompt, sizeof(prompt), "[%s]> ", diretorio_atual_str);
        printf("\n%s", prompt);

        if (fgets(linha_comando, sizeof(linha_comando), stdin) == NULL) {
            printf("\nSaindo (EOF detectado)...\n");
            break; 
        }

        // remove quebra de linha do final do fgets
        linha_comando[strcspn(linha_comando, "\n\r")] = 0;

        // agora pega apensa o primeiro token como comando
        char* comando = strtok(linha_comando, " \t");

        if (comando == NULL) {
            continue;
        }

        // pega todo o resto da linha como string de argumentos
        char *argumentos = strtok(NULL, "");
        // remove espaços em branco do inicio dos argumentos, se houver
        if (argumentos) while (*argumentos == ' ' || *argumentos == '\t') argumentos++;
        // Se após remover os espaços a string ficar vazia, trata como nula
        if (argumentos && *argumentos == '\0') argumentos = NULL;



        if (strcmp(comando, "print") == 0) {
            // A lógica de 'print' é especial e continua analisando os 'argumentos'
            char* subcomando = strtok(argumentos, " \t\n\r");
            if (subcomando == NULL) {
                printf("Comando 'print' incompleto. Uso: 'print superblock', 'print inode <n>', 'print groups'.\n");
            
            } else if (strcmp(subcomando, "superblock") == 0) {
                // Passa o resto dos argumentos para a função validar
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
            comando_info(&sb, num_grupos, argumentos);
        }
        
        else if (strcmp(comando, "attr") == 0) {
            comando_attr(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }
        
        else if (strcmp(comando, "cat") == 0) {
            comando_cat(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

         else if (strcmp(comando, "ls") == 0) {
            comando_ls(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

        else if (strcmp(comando, "cd") == 0) {
            comando_cd(fd, &sb, gdt, &diretorio_atual_inode, diretorio_atual_str, argumentos);
        }

        else if (strcmp(comando, "pwd") == 0){
            comando_pwd(diretorio_atual_str, argumentos);
        }

        else if (strcmp(comando, "touch") == 0) {
            comando_touch(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

        else if (strcmp(comando, "rm") == 0) {
            comando_rm(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

        else if (strcmp(comando, "mkdir") == 0) {
            comando_mkdir(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

        else if (strcmp(comando, "rmdir") == 0){
            comando_rmdir(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

        else if (strcmp(comando, "rename") == 0){
            comando_rename(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }

        else if (strcmp(comando, "help") == 0) {
            imprimir_ajuda();
        } 
        
        else if (strcmp(comando, "exit") == 0 || strcmp(comando, "quit") == 0) {
            printf("Saindo...\n");
            break;
        } 

        else if (strcmp(comando, "cp") == 0) {
            comando_cp(fd, &sb, gdt, diretorio_atual_inode, argumentos);
        }
        
        else {
            printf("Comando desconhecido: '%s'. Digite 'help' para ver a lista de comandos.\n", comando);
        }

    } while (1);

    // LIMPEZA E ENCERRAMENTO
    printf("Liberando recursos e fechando o disco.\n");
    liberar_descritores_grupo(gdt); // Libera a memória alocada para a GDT
    close(fd);                      // Fecha o arquivo da imagem

    return 0;                       // Sucesso
}