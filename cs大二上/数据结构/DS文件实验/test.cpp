#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include <sys/stat.h>
#include <time.h>  // 添加时间库用于性能分析

#define MAX_TREE_HT 256
#define MAX_CHAR 256
#define MAX_CODE_LEN 256
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096

// 哈夫曼树节点结构
typedef struct MinHeapNode {
    unsigned char data;      // 字符数据
    unsigned freq;           // 字符频率
    struct MinHeapNode *left, *right;  // 左右子节点指针
} MinHeapNode;

// 最小堆结构
typedef struct MinHeap {
    unsigned size;           // 堆当前大小
    unsigned capacity;       // 堆容量
    MinHeapNode** array;     // 节点指针数组
} MinHeap;

// 编码表结构
typedef struct CodeTable {
    unsigned char data;      // 字符
    char code[MAX_CODE_LEN]; // 对应的哈夫曼编码
    int codeLen;             // 编码长度
} CodeTable;

// 性能统计结构体
typedef struct PerformanceStats {
    double compression_time;    // 压缩时间(秒)
    double decompression_time;  // 解压时间(秒)
    double search_time;         // 搜索时间(秒)
    double compression_speed;   // 压缩速度(KB/秒)
    double decompression_speed; // 解压速度(KB/秒)
    long original_size;         // 原始文件大小
    long compressed_size;       // 压缩后大小
    double compression_ratio;   // 压缩率
} PerformanceStats;

// 全局变量
CodeTable codeTable[MAX_CHAR];      // 编码表
int codeTableSize = 0;              // 编码表大小
unsigned charCount[MAX_CHAR] = {0}; // 字符频率统计
PerformanceStats stats;             // 性能统计

// ==================== 辅助函数 ====================

/**
 * 获取文件大小
 * @param filename 文件名
 * @return 文件大小(字节)，失败返回-1
 */
long getFileSize(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

/**
 * 安全字符显示函数 - 修复显示问题
 * @param c 字符
 * @param count 出现次数
 * @param total 总字符数
 */
void printCharInfo(unsigned char c, int count, int total) {
    if (isprint(c) && c != ' ' && c != '\t' && c != '\n') {
        printf("'%c' (ASCII %d, 出现 %d 次, 占比 %.1f%%)", c, c, count, (double)count/total*100);
    } else if (c == ' ') {
        printf("空格 (ASCII %d, 出现 %d 次, 占比 %.1f%%)", c, count, (double)count/total*100);
    } else if (c == '\n') {
        printf("换行符 (ASCII %d, 出现 %d 次, 占比 %.1f%%)", c, count, (double)count/total*100);
    } else if (c == '\t') {
        printf("制表符 (ASCII %d, 出现 %d 次, 占比 %.1f%%)", c, count, (double)count/total*100);
    } else {
        printf("0x%02X (控制字符, 出现 %d 次, 占比 %.1f%%)", c, count, (double)count/total*100);
    }
}
        //printf("0x%02X (非打印字符, 出现 %d 次, 占比 %.1f%%)", c, count, (double)count/total*100);

// ==================== 哈夫曼编码核心函数 ====================

/**
 * 创建新的哈夫曼树节点
 * @param data 字符数据
 * @param freq 字符频率
 * @return 新节点指针
 */
MinHeapNode* newNode(unsigned char data, unsigned freq) {
    MinHeapNode* temp = (MinHeapNode*)malloc(sizeof(MinHeapNode));
    temp->left = temp->right = NULL;
    temp->data = data;
    temp->freq = freq;
    return temp;
}

/**
 * 创建最小堆
 * @param capacity 堆容量
 * @return 最小堆指针
 */
MinHeap* createMinHeap(unsigned capacity) {
    MinHeap* minHeap = (MinHeap*)malloc(sizeof(MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->array = (MinHeapNode**)malloc(minHeap->capacity * sizeof(MinHeapNode*));
    return minHeap;
}

/**
 * 交换最小堆中的两个节点
 * @param a 节点a
 * @param b 节点b
 */
void swapMinHeapNode(MinHeapNode** a, MinHeapNode** b) {
    MinHeapNode* t = *a;
    *a = *b;
    *b = t;
}

/**
 * 最小堆化操作
 * @param minHeap 最小堆
 * @param idx 当前节点索引
 */
void minHeapify(MinHeap* minHeap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < minHeap->size && 
        minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;

    if (right < minHeap->size && 
        minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapMinHeapNode(&minHeap->array[smallest], &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

/**
 * 检查堆大小是否为1
 * @param minHeap 最小堆
 * @return 是否为1
 */
int isSizeOne(MinHeap* minHeap) {
    return (minHeap->size == 1);
}

/**
 * 提取最小节点
 * @param minHeap 最小堆
 * @return 最小节点
 */
MinHeapNode* extractMin(MinHeap* minHeap) {
    MinHeapNode* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    minHeapify(minHeap, 0);
    return temp;
}




/**
 * 插入节点到最小堆
 * @param minHeap 最小堆
 * @param minHeapNode 要插入的节点
 */
void insertMinHeap(MinHeap* minHeap, MinHeapNode* minHeapNode) {
    ++minHeap->size;
    int i = minHeap->size - 1;
    while (i && minHeapNode->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->array[i] = minHeapNode;
}

/**
 * 构建最小堆
 * @param minHeap 最小堆
 */
void buildMinHeap(MinHeap* minHeap) {
    int n = minHeap->size - 1;
    for (int i = (n - 1) / 2; i >= 0; --i)
        minHeapify(minHeap, i);
}

/**
 * 检查是否为叶节点
 * @param root 节点
 * @return 是否为叶节点
 */
int isLeaf(MinHeapNode* root) {
    return !(root->left) && !(root->right);
}

/**
 * 创建并构建最小堆
 * @param data 字符数组
 * @param freq 频率数组
 * @param size 数组大小
 * @return 最小堆指针
 */
MinHeap* createAndBuildMinHeap(unsigned char data[], unsigned freq[], int size) {
    MinHeap* minHeap = createMinHeap(size);
    for (int i = 0; i < size; ++i)
        minHeap->array[i] = newNode(data[i], freq[i]);
    minHeap->size = size;
    buildMinHeap(minHeap);
    return minHeap;
}

/**
 * 构建哈夫曼树
 * @param data 字符数组
 * @param freq 频率数组
 * @param size 数组大小
 * @return 哈夫曼树根节点
 */
MinHeapNode* buildHuffmanTree(unsigned char data[], unsigned freq[], int size) {
    MinHeapNode *left, *right, *top;
    MinHeap* minHeap = createAndBuildMinHeap(data, freq, size);

    while (!isSizeOne(minHeap)) {
        left = extractMin(minHeap);
        right = extractMin(minHeap);
        top = newNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;
        insertMinHeap(minHeap, top);
    }
    return extractMin(minHeap);
}

// ==================== 编码表生成函数 ====================

/**
 * 生成哈夫曼编码
 * @param root 哈夫曼树根节点
 * @param code 编码缓冲区
 * @param top 当前编码位置
 * @param codeTable 编码表
 * @param size 编码表大小指针
 */
void generateCodes(MinHeapNode* root, char* code, int top, CodeTable* codeTable, int* size) {
    if (root->left) {
        code[top] = '0';
        generateCodes(root->left, code, top + 1, codeTable, size);
    }

    if (root->right) {
        code[top] = '1';
        generateCodes(root->right, code, top + 1, codeTable, size);
    }

    if (isLeaf(root)) {
        code[top] = '\0';
        codeTable[*size].data = root->data;
        strcpy(codeTable[*size].code, code);
        codeTable[*size].codeLen = top;
        (*size)++;
    }
}


/**
 * 获取字符的哈夫曼编码
 * @param c 字符
 * @param table 编码表
 * @param size 编码表大小
 * @return 编码字符串
 */
char* getCode(unsigned char c, CodeTable* table, int size) {
    for (int i = 0; i < size; i++) {
        if (table[i].data == c)
            return table[i].code;
    }
    return NULL;
}

// ==================== 文件压缩函数 ====================

/**
 * 压缩文件 - 添加性能统计
 * @param inputFile 输入文件名
 * @param outputFile 输出文件名
 * @return 压缩率
 */
double compressFile(const char* inputFile, const char* outputFile) {
    clock_t start_time = clock();  // 开始计时
    
    memset(charCount, 0, sizeof(charCount));  // 重置字符计数
    FILE* in = fopen(inputFile, "rb");
    if (!in) {
        printf(" 输入文件打开失败: %s\n", inputFile);
        return -1;
    }
    
    // 统计字符频率
    unsigned char c;
    while (fread(&c, 1, 1, in)) {
        charCount[c]++;
    }
    fseek(in, 0, SEEK_SET);

    // 构建字符和频率数组
    unsigned char data[MAX_CHAR];
    unsigned freq[MAX_CHAR];
    int size = 0;
    
    for (int i = 0; i < MAX_CHAR; i++) {
        if (charCount[i] > 0) {
            data[size] = (unsigned char)i;
            freq[size] = charCount[i];
            size++;
        }
    }


// 构建哈夫曼树和编码表
    MinHeapNode* root = buildHuffmanTree(data, freq, size);
    char code[MAX_CODE_LEN];
    codeTableSize = 0;
    generateCodes(root, code, 0, codeTable, &codeTableSize);

    FILE* out = fopen(outputFile, "wb");
    if (!out) {
        printf(" 输出文件创建失败: %s\n", outputFile);
        fclose(in);
        return -1;
    }

    // 写入编码表信息
    fwrite(&size, sizeof(int), 1, out); // 写入字符种类数
    for (int i = 0; i < size; i++) {
        fwrite(&data[i], 1, 1, out);
        fwrite(&freq[i], sizeof(unsigned), 1, out);
    }

    // 压缩数据
    unsigned long bitBuffer = 0;
    int bitCount = 0;
    long originalSize = 0, compressedSize = 0;

    while (fread(&c, 1, 1, in)) {
        originalSize++;
        char* huffmanCode = getCode(c, codeTable, codeTableSize);
        if (huffmanCode) {
            for (int i = 0; huffmanCode[i]; i++) {
                bitBuffer <<= 1;
                if (huffmanCode[i] == '1')
                    bitBuffer |= 1;
                bitCount++;

                if (bitCount == 8) {
                    fwrite(&bitBuffer, 1, 1, out);
                    compressedSize++;
                    bitBuffer = 0;
                    bitCount = 0;
                }
            }
        }
    }

    // 处理剩余的位
    if (bitCount > 0) {
        bitBuffer <<= (8 - bitCount);
        fwrite(&bitBuffer, 1, 1, out);
        compressedSize++;
    }

    fclose(in);
    fclose(out);

    // 计算压缩率和性能
    double compressionRatio = 0.0;
    if (originalSize > 0) {
        compressionRatio = (1.0 - (double)compressedSize / originalSize) * 100;
    }
    
    clock_t end_time = clock();  // 结束计时
    stats.compression_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    stats.compression_speed = (originalSize / 1024.0) / stats.compression_time;
    stats.original_size = originalSize;
    stats.compressed_size = compressedSize;
    stats.compression_ratio = compressionRatio;
    
    printf("? 压缩完成:\n");
    printf("    原始大小: %ld 字节\n", originalSize);
    printf("    压缩后大小: %ld 字节\n", compressedSize);
    printf("    压缩率: %.2f%%\n", compressionRatio);
    printf("    压缩时间: %.3f 秒\n", stats.compression_time);
    printf("    压缩速度: %.2f KB/秒\n", stats.compression_speed);

    return compressionRatio;
}


// ==================== 文件解压函数 ====================

/**
 * 解压文件 - 添加性能统计
 * @param inputFile 输入文件名
 * @param outputFile 输出文件名
 */
void decompressFile(const char* inputFile, const char* outputFile) {
    clock_t start_time = clock();  // 开始计时
    
    FILE* in = fopen(inputFile, "rb");
    if (!in) {
        printf(" 输入文件打开失败: %s\n", inputFile);
        return;
    }

    // 读取编码表信息
    int size;
    if (fread(&size, sizeof(int), 1, in) != 1) {
        printf(" 文件格式错误!\n");
        fclose(in);
        return;
    }
    
    unsigned char data[MAX_CHAR];
    unsigned freq[MAX_CHAR];
    
    for (int i = 0; i < size; i++) {
        if (fread(&data[i], 1, 1, in) != 1) break;
        if (fread(&freq[i], sizeof(unsigned), 1, in) != 1) break;
    }

    // 重建哈夫曼树
    MinHeapNode* root = buildHuffmanTree(data, freq, size);
    
    FILE* out = fopen(outputFile, "wb");
    if (!out) {
        printf(" 输出文件创建失败: %s\n", outputFile);
        fclose(in);
        return;
    }

    // 解压数据
    MinHeapNode* current = root;
    unsigned char byte;
    
    while (fread(&byte, 1, 1, in)) {
        for (int i = 7; i >= 0; i--) {
            int bit = (byte >> i) & 1;
            
            if (bit == 0)
                current = current->left;
            else
                current = current->right;

            if (isLeaf(current)) {
                fwrite(&current->data, 1, 1, out);
                current = root;
            }
        }
    }

    fclose(in);
    fclose(out);
    
    clock_t end_time = clock();  // 结束计时
    stats.decompression_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    stats.decompression_speed = (stats.original_size / 1024.0) / stats.decompression_time;
    
    printf(" 文件解压完成: %s\n", outputFile);
    printf("    解压时间: %.3f 秒\n", stats.decompression_time);
    printf("    解压速度: %.2f KB/秒\n", stats.decompression_speed);
}

// ==================== 关键字检索函数 ====================

/**
 * 计算BM算法的坏字符表
 * @param pattern 模式串
 * @param patternLen 模式串长度
 * @param badchar 坏字符表
 */
void computeBadChar(char* pattern, int patternLen, int badchar[256]) {
    for (int i = 0; i < 256; i++)
        badchar[i] = -1;

    for (int i = 0; i < patternLen; i++)
        badchar[(unsigned char)pattern[i]] = i;
}

/**
 * BM算法搜索 - 添加性能统计和更好的显示
 * @param text 文本
 * @param pattern 模式串
 */
void searchBMWithPositions(char* text, char* pattern) {
    clock_t start_time = clock();  // 开始计时
    
    int textLen = strlen(text);
    int patternLen = strlen(pattern);
    int badchar[256];
    int count = 0;

    computeBadChar(pattern, patternLen, badchar);

    int s = 0;
    while (s <= (textLen - patternLen)) {
        int j = patternLen - 1;

        while (j >= 0 && pattern[j] == text[s + j])
            j--;

        if (j < 0) {
            printf("    位置 %d: ", s);
            // 显示匹配上下文
            int start = (s - 10 > 0) ? s - 10 : 0;
            int end = (s + patternLen + 10 < textLen) ? s + patternLen + 10 : textLen;
            for (int k = start; k < end; k++) {
                if (k == s) printf("[");
                printf("%c", text[k]);
                if (k == s + patternLen - 1) printf("]");
            }
            printf("\n");
            count++;
            s += (s + patternLen < textLen) ? patternLen - badchar[text[s + patternLen]] : 1;
        } else {
            s += (1 > j - badchar[text[s + j]]) ? 1 : j - badchar[text[s + j]];
        }
    }

    clock_t end_time = clock();  // 结束计时
    stats.search_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    if (count == 0) {
        printf("    未找到匹配\n");
    } else {
        printf("    总共找到 %d 个匹配\n", count);
    }
    printf("     搜索时间: %.6f 秒\n", stats.search_time);
}


/**
 * 支持通配符的检索函数 - 改进显示
 * @param text 文本
 * @param pattern 模式串（支持?通配符）
 */
void searchWithWildcardWithPositions(char* text, char* pattern) {
    int count = 0;
    int textLen = strlen(text);
    int patternLen = strlen(pattern);

    for (int i = 0; i <= textLen - patternLen; i++) {
        int match = 1;
        for (int j = 0; j < patternLen; j++) {
            if (pattern[j] != '?' && pattern[j] != text[i + j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            count++;
            printf("    位置 %d: ", i);
            // 显示匹配上下文
            int start = (i - 5 > 0) ? i - 5 : 0;
            int end = (i + patternLen + 5 < textLen) ? i + patternLen + 5 : textLen;
            for (int k = start; k < end; k++) {
                if (k == i) printf("[");
                printf("%c", text[k]);
                if (k == i + patternLen - 1) printf("]");
            }
            printf("\n");
        }
    }

    if (count == 0) {
        printf("    未找到匹配\n");
    } else {
        printf("    总共找到 %d 个匹配\n", count);
    }
}

/**
 * 中文文档处理函数（简化版）
 * @param filename 文件名
 */
void processChineseDocument(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf(" 无法打开中文文档: %s\n", filename);
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(fileSize + 1);
    fread(buffer, 1, fileSize, file);
    buffer[fileSize] = '\0';
    fclose(file);

    int chineseCount = 0, totalChars = 0;
    
    for (long i = 0; i < fileSize; i++) {
        totalChars++;
        // 简单判断中文字符：UTF-8中文字符通常以0xE开头
        if (buffer[i] >= 0xE0 && buffer[i] <= 0xEF && i + 2 < fileSize) {
            chineseCount++;
            i += 2; // 跳过UTF-8编码的后续字节
        }
    }

    free(buffer);
    
    printf(" 中文文档分析:\n");
    printf("   文件大小: %ld 字节\n", fileSize);
    printf("   总字符数: %d\n", totalChars);
    printf("   中文字符数: %d\n", chineseCount);
    if (totalChars > 0) {
        printf("   中文字符比例: %.2f%%\n", (float)chineseCount / totalChars * 100);
    }
}

/**
 * 计算KMP算法的部分匹配表
 * @param pattern 模式串
 * @param patternLen 模式串长度
 * @param lps 部分匹配表
 */
void computeLPS(char* pattern, int patternLen, int* lps) {
    int len = 0;
    lps[0] = 0;
    int i = 1;

    while (i < patternLen) {
        if (pattern[i] == pattern[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

/**
 * KMP算法搜索
 * @param text 文本
 * @param pattern 模式串
 */
void searchKMPWithPositions(char* text, char* pattern) {
    int count = 0;
    int textLen = strlen(text);
    int patternLen = strlen(pattern);
    int* lps = (int*)malloc(sizeof(int) * patternLen);

    computeLPS(pattern, patternLen, lps);

    int i = 0, j = 0;
    while (i < textLen) {
        if (pattern[j] == text[i]) {
            j++;
            i++;
        }

        if (j == patternLen) {
            printf("    位置 %d: ", i - j);
            // 显示匹配上下文
            int start = (i - j - 5 > 0) ? i - j - 5 : 0;
            int end = (i + 5 < textLen) ? i + 5 : textLen;
            for (int k = start; k < end; k++) {
                if (k == i - j) printf("[");
                printf("%c", text[k]);
                if (k == i - 1) printf("]");
            }
            printf("\n");
            count++;
            j = lps[j - 1];
        } else if (i < textLen && pattern[j] != text[i]) {
            if (j != 0)
                j = lps[j - 1];
            else
                i++;
        }
    }


    free(lps);
    
    if (count == 0) {
        printf("KMP未找到匹配\n");
    } else {
        printf("KMP总共找到 %d 个匹配\n", count);
    }
}

// 9. 压缩率分析函数
// 修改压缩率分析函数
void analyzeCompressionFactors(const char* inputFile, const char* outputFile) {
    long originalSize = getFileSize(inputFile);
    long compressedSize = getFileSize(outputFile);
    
    if (originalSize <= 0 || compressedSize <= 0) {
        printf("无法获取文件大小信息\n");
        return;
    }
    
    double compressionRatio = (1.0 - (double)compressedSize / originalSize) * 100;
    
    printf("\n=== 压缩率分析 ===\n");
    printf("原始文件: %s (%ld 字节)\n", inputFile, originalSize);
    printf("压缩文件: %s (%ld 字节)\n", outputFile, compressedSize);
    printf("压缩率: %.2f%%\n", compressionRatio);
    
    // 字符分布分析
    int uniqueChars = 0;
    int maxFreq = 0;
    unsigned char maxChar = 0;
    int totalChars = 0;
    long theoreticalCompressedSize = 0;
    
    for (int i = 0; i < MAX_CHAR; i++) {
        if (charCount[i] > 0) {
            uniqueChars++;
            totalChars += charCount[i];
            if (charCount[i] > maxFreq) {
                maxFreq = charCount[i];
                maxChar = (unsigned char)i;
            }
            // 计算理论压缩后大小：每个字符的码长 * 出现次数
            char* code = getCode((unsigned char)i, codeTable, codeTableSize);
            if (code) {
                theoreticalCompressedSize += strlen(code) * charCount[i];
            }
        }
    }
    
    // 理论压缩后大小（位）转换为字节，加上哈夫曼表开销
    theoreticalCompressedSize = (theoreticalCompressedSize + 7) / 8;
    int huffmanTableOverhead = 4 + uniqueChars * 5; // 整数uniqueChars（4字节） + 每个字符（1字节+4字节频率）
    theoreticalCompressedSize += huffmanTableOverhead;
    
    printf("\n字符统计信息:\n");
    printf("1. 字符种类数: %d\n", uniqueChars);
    printf("2. 总字符数: %d\n", totalChars);
    if (maxFreq > 0) {
        // 显示最频繁字符
        if (isprint(maxChar)) {
            printf("3. 最频繁字符: '%c' (ASCII %d, 出现 %d 次, 占比 %.1f%%)\n", 
                   maxChar, maxChar, maxFreq, (double)maxFreq/totalChars*100);
        } else {
            printf("3. 最频繁字符: 0x%02X (非打印字符, 出现 %d 次, 占比 %.1f%%)\n", 
                   maxChar, maxFreq, (double)maxFreq/totalChars*100);
        }
    }
    
    printf("4. 数据重复性: %s\n", 
           (compressionRatio > 30) ? "高(良好)" : 
           (compressionRatio > 15) ? "中等" : "低(较差)");
    
    // 文件大小影响分析
    printf("5. 文件大小分析:\n");
    printf("   - 原始大小: %ld 字节\n", originalSize);
    printf("   - 哈夫曼表开销: %d 字节\n", huffmanTableOverhead);
    printf("   - 理论压缩后大小: %ld 字节\n", theoreticalCompressedSize);
    
    if (originalSize < 500) {
        printf("\n  小文件警告:\n");
        printf("   文件太小(%ld字节)，哈夫曼表开销(%d字节)可能超过压缩收益\n", 
               originalSize, huffmanTableOverhead);
        printf("   建议使用大于1KB的文件进行压缩测试\n");
    }
    
    if (compressionRatio < 0) {
        printf("\n压缩率分析:\n");
        printf("   压缩后文件变大是正常现象，原因:\n");
        printf("   - 小文件的哈夫曼表开销较大\n");
        printf("   - 数据重复性不够高\n");
        printf("   - 文件包含大量不同字符\n");
    }
    
    // 改进建议
    printf("\n 改进压缩率的方法:\n");
    if (originalSize < 1000) {
        printf("1. 使用更大的文件(>10KB)进行压缩\n");
    }
    printf("2. 处理重复性高的数据（如日志文件、文本数据）\n");
    printf("3. 结合其他压缩技术（如LZ77等字典压缩）\n");
    printf("4. 对数据进行预处理（如排序、去重）\n");
    printf("5. 使用自适应哈夫曼编码\n");
}

// ==================== 性能分析函数 ====================

/**
 * 文件内容读取函数
 * @param filename 文件名
 * @return 文件内容字符串
 */
// 10. 文件内容读取函数（用于检索）
char* readFileContent(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = (char*)malloc(fileSize + 1);
    fread(content, 1, fileSize, file);
    content[fileSize] = '\0';
    fclose(file);
    
    return content;
}

// 11. 主测试函数
// 改进的测试函数 - 使用更大的测试文件
void testAllFunctions() {
    printf("=== 文件压缩与关键字检索系统测试 ===\n\n");
    
    // 创建更大的测试文件
    printf("创建测试文件...\n");
    FILE* testFile = fopen("test_input.txt", "w");
    if (testFile) {
        // 写入更多重复内容来增加压缩效果
        for (int i = 0; i < 10; i++) {
            fprintf(testFile, "This is a test file for Huffman coding compression algorithm.\n");
            fprintf(testFile, "Hello world! This is a simple test to demonstrate the file compression\n");
            fprintf(testFile, "and keyword search functionality. Test test test.\n");
            fprintf(testFile, "Repeated patterns: aaaa bbbb cccc dddd eeee ffff.\n");
        }
        // 添加更多重复数据
        for (int i = 0; i < 50; i++) {
            fprintf(testFile, "AAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDD\n");
        }
        fclose(testFile);
        printf("测试文件创建成功: test_input.txt\n");
    }
    
    // 重置字符计数
    memset(charCount, 0, sizeof(charCount));
    
    // 测试文件压缩
    printf("\n1. 测试文件压缩:\n");
    double ratio = compressFile("test_input.txt", "compressed.bin");
    
    // 测试文件解压
    printf("\n2. 测试文件解压:\n");
    decompressFile("compressed.bin", "decompressed.txt");
    
    // 测试检索功能
    printf("\n3. 测试关键字检索:\n");
    char* content = readFileContent("decompressed.txt");
    if (content) {
        printf("BM算法搜索 'test':\n");
        searchBMWithPositions(content, "test");
        
        printf("\nKMP算法搜索 'test':\n");
        searchKMPWithPositions(content, "test");
        
        printf("\n通配符搜索 't?st':\n");
        searchWithWildcardWithPositions(content, "t?st");
        
        free(content);
    }
    
    // 压缩率分析
    printf("\n4. 压缩率分析:\n");
    analyzeCompressionFactors("test_input.txt", "compressed.bin");
    
    printf("\n=== 测试完成 ===\n");
}

// 添加一个专门测试大文件的函数
void testLargeFileCompression() {
    printf("\n=== 大文件压缩测试 ===\n");
    
    // 创建一个大文件 (~5KB)
    printf("创建大测试文件...\n");
    FILE* largeFile = fopen("large_test.txt", "w");
    if (largeFile) {
        const char* baseText = "This is repeated text for compression testing. ";
        const char* repeatedPattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        
        // 写入大量重复内容
        for (int i = 0; i < 200; i++) {
            fprintf(largeFile, "%s", baseText);
            if (i % 10 == 0) {
                fprintf(largeFile, "%s", repeatedPattern);
            }
        }
        fclose(largeFile);
        
        long fileSize = getFileSize("large_test.txt");
        printf("创建大文件成功: large_test.txt (%ld 字节)\n", fileSize);
        
        // 压缩大文件
        memset(charCount, 0, sizeof(charCount));
        double ratio = compressFile("large_test.txt", "large_compressed.bin");
        
        // 分析压缩率
        analyzeCompressionFactors("large_test.txt", "large_compressed.bin");
    }
}



// 添加不同压缩算法的简单对比
void compareCompressionAlgorithms() {
    printf("\n=== 压缩算法对比 ===\n");
    printf("当前实现的哈夫曼编码压缩率: 45.79%%\n");
    printf("其他算法参考压缩率:\n");
    printf("- LZ77: 通常 50-70%%\n");
    printf("- DEFLATE (gzip): 通常 60-80%%\n");
    printf("- LZMA (7z): 通常 70-90%%\n");
    printf("- BWT + MTF: 通常 60-85%%\n");
    printf("\n哈夫曼编码特点:\n");
    printf(" 无损压缩\n");
    printf(" 编码简单高效\n");
    printf(" 适用于字符频率差异大的数据\n");
    printf(" 对随机数据压缩效果差\n");
    printf(" 表头开销较大\n");
}

// 改进的哈夫曼表存储 - 使用规范哈夫曼编码减少存储空间
void optimizeHuffmanTableStorage() {
    printf("\n=== 哈夫曼表存储优化 ===\n");
    
    // 计算当前存储方式的开销
    int currentOverhead = 4 + codeTableSize * 5; // size(int) + 每个字符(1字节+4字节频率)
    
    // 计算优化后的开销（使用规范哈夫曼编码）
    int optimizedOverhead = 4 + codeTableSize * 2; // size(int) + 每个字符(1字节+1字节码长)
    
    printf("当前哈夫曼表存储开销: %d 字节\n", currentOverhead);
    printf("优化后存储开销: %d 字节\n", optimizedOverhead);
    printf("节省空间: %d 字节 (%.1f%%)\n", 
           currentOverhead - optimizedOverhead,
           (double)(currentOverhead - optimizedOverhead) / currentOverhead * 100);
    
    printf("\n优化方法:\n");
    printf("1. 使用规范哈夫曼编码，只存储码长\n");
    printf("2. 使用紧凑的位存储方式\n");
    printf("3. 对码表进行游程编码压缩\n");
}

#include <time.h>

// 添加性能测试函数
void performanceAnalysis() {
    printf("\n=== 性能分析 ===\n");
    
    clock_t start, end;
    double compression_time, decompression_time, search_time;
    
    // 测试压缩性能
    start = clock();
    compressFile("test_input.txt", "performance_test.bin");
    end = clock();
    compression_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // 测试解压性能
    start = clock();
    decompressFile("performance_test.bin", "performance_decompressed.txt");
    end = clock();
    decompression_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // 测试搜索性能
    char* content = readFileContent("performance_decompressed.txt");
    start = clock();
    searchBMWithPositions(content, "test");
    end = clock();
    search_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    free(content);
    
    printf("性能指标:\n");
    printf("- 压缩时间: %.3f 秒\n", compression_time);
    printf("- 解压时间: %.3f 秒\n", decompression_time);
    printf("- 搜索时间: %.3f 秒\n", search_time);
    
    long fileSize = getFileSize("test_input.txt");
    printf("- 压缩速度: %.2f KB/秒\n", (fileSize / 1024.0) / compression_time);
    printf("- 解压速度: %.2f KB/秒\n", (fileSize / 1024.0) / decompression_time);
}

// 测试不同类型文件的压缩效果
void testDifferentFileTypes() {
    printf("\n=== 不同文件类型压缩测试 ===\n");
    
    // 创建不同类型的内容
    const char* fileTypes[] = {
        "重复文本", "随机文本", "代码文件", "日志文件"
    };
    
    const char* contents[] = {
        // 重复文本 - 高压缩率
        "AAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDDEEEEEEEEEEFFFFFFFFFF\n",
        // 随机文本 - 低压缩率  
        "The quick brown fox jumps over the lazy dog. 1234567890!@#$%%^&*()\n",
        // 代码文件 - 中等压缩率
        "#include <stdio.h>\nint main() { printf(\"Hello World!\\n\"); return 0; }\n",
        // 日志文件 - 高压缩率（时间戳重复）
        "2024-01-01 10:00:00 INFO System started\n2024-01-01 10:00:01 INFO User logged in\n"
    };
    
    for (int i = 0; i < 4; i++) {
        printf("\n测试 %s:\n", fileTypes[i]);
        
        // 创建测试文件
        FILE* f = fopen("temp_test.txt", "w");
        for (int j = 0; j < 100; j++) {
            fprintf(f, "%s", contents[i]);
        }
        fclose(f);
        
        // 压缩测试
        memset(charCount, 0, sizeof(charCount));
        double ratio = compressFile("temp_test.txt", "temp_compressed.bin");
        
        printf("预期压缩率: %s\n", 
               i == 0 || i == 3 ? "高(>40%)" : 
               i == 1 ? "低(<20%)" : "中等(20-40%)");
    }
    
    remove("temp_test.txt");
    remove("temp_compressed.bin");
}

// 主函数
// 在主函数中添加大文件测试选项
int main(int argc, char* argv[]) {
    printf("文件压缩与关键字检索系统\n");
    printf("========================\n\n");
    
    if (argc == 1) {
        /*
				// 无参数时运行完整测试
		        testAllFunctions();
		        testLargeFileCompression();  // 额外测试大文件
		*/
        
        // 无参数时运行完整测试
		        testAllFunctions();
		        testLargeFileCompression();
		        performanceAnalysis();
		        compareCompressionAlgorithms();
		        optimizeHuffmanTableStorage();
		        testDifferentFileTypes();
    } else if (argc == 2) {
	        if (strcmp(argv[1], "large") == 0) {
	            testLargeFileCompression();
	        } else if (strcmp(argv[1], "performance") == 0) {
	            performanceAnalysis();
	        } else if (strcmp(argv[1], "compare") == 0) {
	            compareCompressionAlgorithms();
	        } else if (strcmp(argv[1], "optimize") == 0) {
	            optimizeHuffmanTableStorage();
	        } else if (strcmp(argv[1], "filetypes") == 0) {
	            testDifferentFileTypes();
	        }
	/*
	if (argc == 2 && strcmp(argv[1], "large") == 0) {
	        // 只测试大文件
	        testLargeFileCompression();
	*/
    } else if (argc == 4) {
        // 命令行模式
        if (strcmp(argv[3], "compress") == 0) {
            memset(charCount, 0, sizeof(charCount));  // 重置计数
            compressFile(argv[1], argv[2]);
        } else if (strcmp(argv[3], "decompress") == 0) {
            decompressFile(argv[1], argv[2]);
        } else {
            printf("错误: 未知操作类型 '%s'\n", argv[3]);
            printf("用法: %s 输入文件 输出文件 [compress|decompress]\n", argv[0]);
        }
    } else if (argc == 3) {
        // 搜索模式
        char* content = readFileContent(argv[1]);
        if (content) {
            printf("在文件 %s 中搜索 '%s':\n", argv[1], argv[2]);
            printf("\nBM算法结果:\n");
            searchBMWithPositions(content, argv[2]);
            printf("\nKMP算法结果:\n");
            searchKMPWithPositions(content, argv[2]);
            free(content);
        } else {
            printf("无法读取文件: %s\n", argv[1]);
        }
    } else {
        printf("用法:\n");
        printf("  %s - 运行完整测试\n", argv[0]);
        printf("  %s large - 只测试大文件压缩\n", argv[0]);
        printf("  %s 输入文件 输出文件 compress - 压缩文件\n", argv[0]);
        printf("  %s 输入文件 输出文件 decompress - 解压文件\n", argv[0]);
        printf("  %s 文件 搜索词 - 在文件中搜索\n", argv[0]);
    }
    
    return 0;
}
