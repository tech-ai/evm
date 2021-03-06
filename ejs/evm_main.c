/****************************************************************************
**
** Copyright (C) 2020 @scriptiot
**
**  EVM是一款通用化设计的虚拟机引擎，拥有语法解析前端接口、编译器、虚拟机和虚拟机扩展接口框架。
**  支持js、python、qml、lua等多种脚本语言，纯Ｃ开发，零依赖，内置REPL，支持主流 ROM > 40KB, RAM > 2KB的MCU;
**  自带垃圾回收（GC）先进的内存管理，采用最复杂的压缩算法，无内存碎片（大部分解释器都存在内存碎片）
**  Version	: 1.0
**  Email	: scriptiot@aliyun.com
**  Website	: https://github.com/scriptiot/evm
**            https://gitee.com/scriptiot/evm
**  Licence: Apache-2.0
****************************************************************************/

#include "evm_main.h"
#include "ecma.h"
#include "uol_output.h"
#include <drivers/gpio.h>

evm_t * nevm_runtime;

char evm_repl_tty_read(evm_t * e)
{
    EVM_UNUSED(e);
    char c = console_getchar();
    return c;
}

const char * vm_load(evm_t * e, char * path, int type){
    if( !strcmp(path, "uol_output.ubc") ){
        return uol_binary_buf;
    }
    return NULL;
}

void * vm_malloc(int size)
{
    void * m = malloc(size);
    if(m) memset(m, 0 ,size);
    return m;
}

void vm_free(void * mem)
{
    if(mem) free(mem);
}

int nevm_runtime_setup(void){
    nevm_runtime = (evm_t*)malloc(sizeof(evm_t));
    memset(nevm_runtime, 0, sizeof(evm_t));
    int err = evm_init(nevm_runtime, NEVM_HEAP_SIZE, NEVM_STACK_SIZE, NEVM_MODULE_SIZE, EVM_VAR_NAME_MAX_LEN, EVM_FILE_NAME_LEN);

    err = evm_boot(nevm_runtime, "uol_output.ubc");
    if (err != ec_ok )  {
        printk("Unable to boot uol runtime");
        return err;
    }
    err = nevm_start(nevm_runtime);
    if(err) {return err;}
    printk("heap usage = %d\r\n", nevm_runtime->heap->free);
    return ec_ok;
}

const struct uart_config uart_cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1, 
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

void console_setup(){
    struct device *uart_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
    uart_configure(uart_dev, &uart_cfg);
    console_init();	
}

int evm_main(void){
    console_setup();

    evm_register_free((intptr_t)vm_free);
    evm_register_malloc((intptr_t)vm_malloc);
    evm_register_print((intptr_t)printk);
    evm_register_file_load((intptr_t)vm_load);

    if( nevm_runtime_setup() != ec_ok ){
        printk("Failed to setup uol runtime\r\n");
        return ec_err;
    }

    evm_t * env = (evm_t*)malloc(sizeof(evm_t));
    memset(env, 0, sizeof(evm_t));
    int err = evm_init(env, EVM_HEAP_SIZE, EVM_STACK_SIZE, EVM_MODULE_SIZE, EVM_VAR_NAME_MAX_LEN, EVM_FILE_NAME_LEN);
    if( err ) {
        evm_print("Failed to initialize evm\r\n");
        return err;
    }
    err = ecma_module(env);
    if( err ) {
        evm_print("Failed to add ecma module\r\n");
    }
    err = evm_module(env);
    if( err ) {
        evm_print("Failed to add evm module\r\n");
        return err;
    }

    err = evm_repl_run(env, 20, EVM_LANG_JS);
    return err;
}