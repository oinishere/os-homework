#include "os.h"

/*
 * 引用 page.c 中的函數
 * 假設 page_alloc 和 page_free 已經在那邊定義好了，這裡只需 extern
 */
extern void *page_alloc(int npages);
// 在此實作中，我們主要透過 page_alloc 獲取記憶體池，
// 細粒度的 free 通常只將記憶體還給 heap list，而不直接還給 page allocator，
// 除非實作了非常複雜的頁面邊界檢查。

/*
 * 定義區塊標頭 (Header)
 * 每個由 malloc 分配出去或在庫中的區塊都有這個標頭
 */
struct block {
    uint32_t size;       // 該區塊的數據區域大小 (不包含 sizeof(struct block))
    struct block *next;  // 指向下一個空閒區塊 (僅在該區塊空閒時使用)
};

/* 
 * 定義對齊大小。
 * 在 32 位元系統通常是 4 bytes，64 位元是 8 bytes。
 * 為了安全與效能，我們強制對齊。
 */
#define ALIGN_SIZE 4
#define BLOCK_SIZE sizeof(struct block)

/* page.c 中定義的 PAGE_SIZE 為 256 */
#define PAGE_SIZE 256 

/* 全域變數：指向空閒鏈結串列的開頭 */
static struct block *free_list = NULL;

/*
 * 工具函數：將 size 對齊到 ALIGN_SIZE 的倍數
 */
static uint32_t align(uint32_t size)
{
    if (size % ALIGN_SIZE == 0)
        return size;
    return size + (ALIGN_SIZE - (size % ALIGN_SIZE));
}

/*
 * 工具函數：向 page allocator 請求更多記憶體並加入 free_list
 * size: 包含 Header 在內的總需求大小 (Bytes)
 */
static void request_more_pages(uint32_t required_size)
{
    // 計算需要多少個頁面
    int npages = required_size / PAGE_SIZE;
    if (required_size % PAGE_SIZE != 0) {
        npages++;
    }

    // 向底層 page_alloc 申請記憶體
    void *p = page_alloc(npages);
    if (!p) {
        return; // Out of memory
    }

    // 初始化新申請的區塊
    struct block *new_block = (struct block *)p;
    
    // 計算實際獲得的總大小 (Bytes) - 扣除 Header
    new_block->size = (npages * PAGE_SIZE) - BLOCK_SIZE;
    new_block->next = NULL;

    // 將新區塊透過 free() 函數加入到我們的 free_list 中
    // 這樣可以複用 free 中的合併(coalescing)邏輯
    // 注意：這裡傳入的是數據區的指標 (header + 1)
    free((void *)(new_block + 1));
}

void *malloc(size_t size)
{
    struct block *curr, *prev;
    uint32_t aligned_size = align(size);

    // 1. 邊界檢查
    if (size == 0) return NULL;

    // 2. 嘗試從 free_list 中尋找適合的區塊 (First-fit)
    // 我們可能需要嘗試兩次：第一次失敗後會 request_more_pages，然後再試一次
    for (int attempt = 0; attempt < 2; attempt++) {
        curr = free_list;
        prev = NULL;

        while (curr != NULL) {
            if (curr->size >= aligned_size) {
                // 找到足夠大的區塊
                
                // 檢查是否值得分割 (Split)
                // 如果剩餘空間足夠容納一個 Header + 最小數據單元，就分割
                if (curr->size >= aligned_size + BLOCK_SIZE + ALIGN_SIZE) {
                    struct block *new_free_block = (struct block *)((uint8_t *)curr + BLOCK_SIZE + aligned_size);
                    
                    // 計算剩餘區塊的大小
                    new_free_block->size = curr->size - aligned_size - BLOCK_SIZE;
                    new_free_block->next = curr->next;

                    // 更新當前區塊大小
                    curr->size = aligned_size;
                    // 更新鏈結
                    if (prev) prev->next = new_free_block;
                    else      free_list = new_free_block;
                } else {
                    // 不分割，直接移除該節點
                    if (prev) prev->next = curr->next;
                    else      free_list = curr->next;
                }

                // 返回數據區域的指標 (跳過 Header)
                return (void *)(curr + 1);
            }
            prev = curr;
            curr = curr->next;
        }

        // 如果第一次遍歷沒找到，請求更多頁面
        if (attempt == 0) {
            request_more_pages(aligned_size + BLOCK_SIZE);
        }
    }

    // 如果還是沒有記憶體
    return NULL;
}

void free(void *ptr)
{
    if (!ptr) return;

    // 1. 取得 Header
    // 指標往回退 sizeof(struct block) 找到標頭
    struct block *block_to_free = ((struct block *)ptr) - 1;
    
    /* 
     * 2. 將區塊插回 free_list 並按地址排序
     * 按地址排序是為了方便合併相鄰區塊
     */
    struct block *curr = free_list;
    struct block *prev = NULL;

    // 尋找插入點：prev < block_to_free < curr
    while (curr != NULL && curr < block_to_free) {
        prev = curr;
        curr = curr->next;
    }

    // 插入
    block_to_free->next = curr;
    if (prev) {
        prev->next = block_to_free;
    } else {
        free_list = block_to_free;
    }

    // 3. 合併 (Coalescing)
    
    // 嘗試與後一個區塊合併 (Right Coalescing)
    // 檢查：當前區塊地址 + header + size 是否等於下一個區塊的地址
    if (curr != NULL) {
        uint8_t *ptr_end = (uint8_t *)block_to_free + BLOCK_SIZE + block_to_free->size;
        if (ptr_end == (uint8_t *)curr) {
            block_to_free->size += BLOCK_SIZE + curr->size;
            block_to_free->next = curr->next;
        }
    }

    // 嘗試與前一個區塊合併 (Left Coalescing)
    if (prev != NULL) {
        uint8_t *ptr_end = (uint8_t *)prev + BLOCK_SIZE + prev->size;
        if (ptr_end == (uint8_t *)block_to_free) {
            prev->size += BLOCK_SIZE + block_to_free->size;
            prev->next = block_to_free->next;
        }
    }
}