在 xv6 项目中调试内核需要使用两个终端窗口。根据你的环境检测结果，你已经安装了 `gdb-multiarch`，这是调试 RISC-V 程序的推荐工具。

请按照以下步骤操作：

### 步骤 1：启动 QEMU 等待调试连接

在第一个终端中，运行以下命令：

```bash
make qemu-gdb
```

- 这个命令会编译内核（如果需要），生成 `.gdbinit` 配置文件，并启动 QEMU。
- 你会看到 QEMU 启动并暂停，等待 GDB 连接。输出中会显示类似 `tcp::2xxxx` 的端口号。

### 步骤 2：启动 GDB

在第二个终端中，确保你位于 xv6 项目的根目录（`/home/rhy/Desktop/MyNotes/xv6`），然后运行：

```bash
gdb-multiarch
```

- GDB 启动时会自动读取当前目录下的 `.gdbinit` 文件。
- 它会自动连接到 QEMU，并加载 `kernel/kernel` 的符号表。
- 连接成功后，你应该会看到 GDB 提示符，并且程序暂停在入口点附近。

### 常用调试命令

进入 GDB 后，你可以使用标准的调试命令：

- `c` (continue): 继续执行代码。
- `b function_name` (break): 在函数处设置断点，例如 `b exec`。
- `b file.c:line` (break): 在特定文件的特定行设置断点，例如 `b kernel/sysproc.c:15`。
- `si` (stepi): 单步执行汇编指令。
- `n` (next): 单步执行源代码（不进入函数）。
- `s` (step): 单步执行源代码（进入函数）。
- `layout src`: 显示源代码窗口（非常有用）。
- `layout asm`: 显示汇编代码窗口。
- `p variable` (print): 打印变量值。

### 常见问题

如果启动 `gdb-multiarch` 时提示 __"warning: File ".../.gdbinit" auto-loading has been declined"__，你需要允许 GDB 加载该文件。 解决方法：在你的用户主目录创建或编辑 `~/.gdbinit` 文件，添加以下内容：

```javascript
add-auto-load-safe-path /home/rhy/Desktop/MyNotes/xv6/.gdbinit
```

或者直接在 GDB 中输入 `source .gdbinit` 手动加载。
