// 作业一：
// 编写C程序，实现文件内容替换功能：
// 1. 查找并替换文件中特定内容；
// 2. 使用文件IO
// 程序执行的命令形如： 
// rep  file findstr repstr
// file： 指定的文本文件
// findstr: 待查找的字符串
// repstr：替换后的字符串
// 例如： 
// rep test.txt "$home" "/cug/home"
// 表示 将文件 test.txt 中的字符串 "$home" 替换为 "/home/cug"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

void replaceInFile(const char *filename, const char *findStr,const char *repStr) 
{
    int fd = open(filename, O_RDWR); // 打开文件以读写方式
    if (fd == -1) 
    {
        perror("打开文件错误");
        return;
    }

    struct stat fileStat;
    if (fstat(fd, &fileStat) == -1) {
        perror("获取文件大小错误");
        close(fd);
        return;
    }

    off_t fileSize = fileStat.st_size;
    char *buffer = (char *)malloc(fileSize);
    ssize_t bytesRead = read(fd, buffer, fileSize);

    if (bytesRead == -1) {
        perror("读文件错误");
        close(fd);
        free(buffer);
        return;
    }

    char *pos = buffer;
    ssize_t findStrLen = strlen(findStr);
    ssize_t repStrLen = strlen(repStr);

    while ((pos = strstr(pos, findStr)) != NULL) 
    {
        // 计算需要移动的字节数
        ssize_t bytesToMove = bytesRead - (pos - buffer) - findStrLen;
        // 计算新的文件大小
         fileSize = fileSize - findStrLen + repStrLen;
        // 调整文件大小
        if (ftruncate(fd, fileSize) == -1) 
        {
            perror("增加文件大小错误");
            close(fd);
            free(buffer);
            return;
        }
        // 移动后续字符
        memmove(pos + repStrLen, pos + findStrLen, bytesToMove);
        // 替换字符串
        memcpy(pos, repStr, repStrLen);
        // 继续查找下一个匹配
        pos += repStrLen;
        bytesRead = fileSize;
    }

    // 移动文件指针到文件开头，以便写入替换后的内容
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("移动文件指针错误");
        close(fd);
        free(buffer);
        return;
    }

    // 写入替换后的内容
    if (write(fd, buffer, bytesRead) == -1) 
    {
        perror("写入文件错误");
        return;
    }

    close(fd);
    free(buffer);
    printf("字符替换成功！\n");
    return;
}

int main(int argc, const char *argv[]) 
{
    if (argc != 4) {
        printf("用法: ./rep 文件名 查找字符串 替换字符串\n");
        return 1;
    }

    const char *filename = argv[1];
    const char *findStr = argv[2];
    const char *repStr = argv[3];

    replaceInFile(filename, findStr, repStr);

    return 0;
}
