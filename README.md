# EXT2Shell – Sistema de Arquivos EXT2 em C

Este projeto consiste na implementação de um interpretador de comandos (shell) para manipulação de uma imagem de disco no formato EXT2, desenvolvido como parte da disciplina de Sistemas Operacionais do curso de Ciência da Computação na UTFPR.

## Objetivo

Permitir a leitura e escrita em uma imagem de disco EXT2 através de comandos similares aos de um terminal Linux. A imagem é montada virtualmente e manipulada sem utilizar bibliotecas externas ou funções do sistema (como `system()` ou `exec()`), respeitando as estruturas originais do EXT2.

## Como compilar

Certifique-se de estar em um sistema Linux com `make` e `gcc` instalados.

```bash
make
```

O binário será gerado na pasta `bin/` com o nome `ext2shell`.


##  Como criar uma imagem EXT2
Para criar uma imagem EXT2, você pode usar o comando `dd` para criar um arquivo de imagem e, em seguida, formatá-lo com `mkfs.ext2`. Aqui está um exemplo:

```bash
dd if=/dev/zero of=myext2image.img bs=1024 count=64K
mkfs.ext2 -b 1024 myext2image.img
```
Para verificar a integridade de um sistema EXT2, você pode usar o comando `e2fsck`:

```bash
e2fsck myext2image.img
```

## Como executar

```bash
./bin/ext2shell caminho/para/sua/imagem.img
```

Exemplo:

```bash
./bin/ext2shell myext2image.img
```

A shell será iniciada com o diretório raiz `/` da imagem.

## 🧭 Comandos disponíveis

| Comando | Descrição |
|---------|-----------|
| `ls [caminho]` | Lista arquivos e diretórios no caminho atual ou especificado. |
| `cd <caminho>` | Navega para outro diretório. |
| `pwd` | Mostra o caminho absoluto do diretório atual. |
| `cat <arquivo>` | Mostra o conteúdo de um arquivo texto. |
| `attr <arquivo \| diretório>` | Exibe os atributos do inode (modo, datas, etc). |
| `info` | Mostra as informações do superbloco e da imagem EXT2. |
| `touch <arquivo>` | Cria um novo arquivo vazio. |
| `mkdir <diretório>` | Cria um novo diretório. |
| `rename <antigo> <novo>` | Renomeia arquivos ou diretórios. |
| `rm <arquivo>` | Remove um arquivo. |
| `rmdir <diretório>` | Remove um diretório vazio. |
| `cp <origem_na_imagem> <destino_no_sistema>` | Copia arquivos da imagem para seu sistema real. |
| `print superblock` | Imprime o conteúdo bruto do superbloco. |
| `print inode <num>` | Imprime os dados brutos de um inode específico. |
| `print groups` | Imprime os descritores de grupo. |
| `help` | Exibe todos os comandos disponíveis. |
| `exit` ou `quit` | Encerra a shell. |



## 📌 Exemplos de uso

```bash
[/]> ls
. ..
hello.txt
documentos
imagens

[/]> cat hello.txt
Olá, mundo!

[/]> mkdir testes
Diretório 'testes' criado com sucesso.

[/]> touch testes/novo.txt
Arquivo 'testes/novo.txt' criado com sucesso.

[/]> attr testes/novo.txt
Tamanho: 0 bytes
Permissões: rw-r--r--
Inode: 42
Criado em: 2025-07-05 23:59:00

[/]> cp hello.txt /home/usuario/Desktop
Arquivo copiado com sucesso!
```



## 🛠️ Funcionalidades implementadas

- Leitura de superbloco, inode e descritores de grupo
- Manipulação de diretórios e arquivos (leitura e escrita)
- Criação e remoção de arquivos e diretórios
- Suporte a paths relativos e absolutos
- Shell interativa com parser próprio



##  Limitações conhecidas

- Os diretórios são limitados a 1 bloco (1024 bytes).
- Arquivos maiores que 64 MiB não são suportados.
- O `touch` não atualiza a data de arquivos existentes, apenas cria novos.



##  Equipe

- **Allan Custódio Diniz Marques** 
- **Felipe Kenzo Suguimoto**
- **Vitor Hugo Melo Ribeiro**

##  Referências

- OSDev.org – EXT2 overview  
- CARD, TWEEDIE e TS’O – Design and Implementation of the EXT2  
- ALTIERI & HOWE – The EXT2 Filesystem  
- POIRIER – The Second Extended File System  


##  Licença

Projeto acadêmico para fins educacionais — UTFPR, 2025.
