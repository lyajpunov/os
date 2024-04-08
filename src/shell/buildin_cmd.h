/*
 * @Author: lyajpunov 1961558693@qq.com
 * @Date: 2024-04-08 02:56:55
 * @LastEditors: lyajpunov 1961558693@qq.com
 * @LastEditTime: 2024-04-08 05:13:51
 * @FilePath: /os/src/shell/buildin_cmd.h
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#ifndef __SHELL_BUILDIN_CMD_H
#define __SHELL_BUILDIN_CMD_H

#include "stdin.h"

void make_clear_abs_path(char* path, char* wash_buf);
void buildin_pwd(uint32_t argc, char** argv);
char* buildin_cd(uint32_t argc, char** argv);
void buildin_ls(uint32_t argc, char** argv);
int32_t buildin_mkdir(uint32_t argc, char** argv);
int32_t buildin_rmdir(uint32_t argc, char** argv);
int32_t buildin_rm(uint32_t argc, char** argv);
void buildin_touch(uint32_t argc, char** argv);
void buildin_ps(uint32_t argc, char** argv);
void buildin_clear(uint32_t argc, char** argv);

#endif
