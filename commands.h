#ifndef COMMANDS_H
#define COMMANDS_H

#include "headers.h"

// --- 'print' ---
void comando_print_superblock(const superbloco* sb);
void comando_print_inode(int fd, const superbloco* sb, const group_desc* gdt, char* arg_num_inode);
void comando_print_groups(const group_desc* gdt, uint32_t num_grupos);

// --- info ---
void comando_info(const superbloco* sb, uint32_t num_grupos, char* argumentos);

// --- attr ---
void comando_attr(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- cat ---
void comando_cat(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- ls ---
void comando_ls(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- pwd ---
void comando_pwd(const char* diretorio_atual_str, char* argumentos);

// --- cd ---
void comando_cd(int fd, const superbloco* sb, const group_desc* gdt, uint32_t* p_inode_dir_atual, char* diretorio_atual_str, char* argumentos);

// --- touch ---
void comando_touch(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- rm ---
void comando_rm(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- mkdir ---
void comando_mkdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- rmdir ---
void comando_rmdir(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- rename ---
void comando_rename(int fd, const superbloco* sb, const group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);

// --- cp ---
void comando_cp(int fd, superbloco* sb, group_desc* gdt, uint32_t inode_dir_atual, char* argumentos);
#endif