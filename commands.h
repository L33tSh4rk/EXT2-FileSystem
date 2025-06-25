#ifndef COMMANDS_H
#define COMMANDS_H

#include "headers.h"

// --- 'print' ---
void comando_print_superblock(const superbloco* sb);
void comando_print_inode(int fd, const superbloco* sb, const group_desc* gdt, char* arg_num_inode);
void comando_print_groups(const group_desc* gdt, uint32_t num_grupos);

// --- info ---
void comando_info(const superbloco* sb, uint32_t num_grupos);

// --- attr ---
void comando_attr(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg);

// --- cat ---
void comando_cat(int fd, const superbloco* sb, const group_desc* gdt, char* caminho_arg);

// --- ls ---
void comando_ls(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg);

// --- pwd ---
void comando_pwd(const char* diretorio_atual_str);

// --- cd ---
void comando_cd(int fd, const superbloco* sb, const group_desc* gdt,
                uint32_t* p_inode_dir_atual, char* diretorio_atual_str,
                char* caminho_arg);

// --- touch ---
void comando_touch(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg);

// --- rm ---
void comando_rm(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg);

// --- mkdir ---
void comando_mkdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg);

// --- rmdir ---
void comando_rmdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* caminho_arg);

// --- rename ---
void comando_rename(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* nome_antigo_arg, char* nome_novo_arg);

#endif