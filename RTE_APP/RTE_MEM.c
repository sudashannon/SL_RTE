/**
  ******************************************************************************
  * @file    RTE_MEM.h
  * @author  Shan Lei ->>lurenjia.tech ->>https://github.com/sudashannon
  * @brief   ��̬�ڴ�������Թ������ڴ棬��ֲ��lvgl��
  * @version V1.1 2019/01/08
	* @History V1.0 ��������ֲ���޸���lvgl
	           V1.1 �����˻�����
						 V1.2 �޸��˻������߼�
  ******************************************************************************
  */
#include "RTE_MEM.h"
#if RTE_USE_MEMMANAGE == 1
#include "RTE_Math.h"
#include "RTE_Log.h"
#include "RTE_UString.h"
#define MEM_STR "[MEM]"
/*************************************************
*** ����BGet�Ľṹ���������̬����
*************************************************/
static RTE_MEM_t MemoryControlHandle[MEM_N] = 
{
	{
		.MemName = MEM_RTE,
		.work_mem = (void *)0,
		.totalsize = 0,
	},
	{
		.MemName = MEM_DMA,
		.work_mem = (void *)0,
		.totalsize = 0,
	},
};
static RTE_MEM_Ent_t  *ent_get_next(RTE_MEM_Name_e mem_name,RTE_MEM_Ent_t * act_e);
static void *ent_alloc(RTE_MEM_Ent_t * e, uint32_t size);
static void ent_trunc(RTE_MEM_Ent_t * e, uint32_t size);
static uint32_t zero_mem;       /*Give the address of this variable if 0 byte should be allocated*/
/*************************************************
*** Args:   mem_name �ƶ���MEMNAME;
            *buf ʵ��ռ�õ��ڴ��ַ��
            len ʵ��ռ�õ��ڴ��С��
*** Function: ���ݻ�ȡ���ڴ��Լ���С��ʼ�������
*************************************************/
//static const osMutexAttr_t MutexIDStdioAttr[MEM_N] = {
//	{
//		"Mem0Mutex",     // human readable mutex name
//		osMutexPrioInherit|osMutexRecursive  ,    					// attr_bits
//		NULL,                // memory for control block   
//		0U                   // size for control block
//	},
//	{
//		"Mem1Mutex",     // human readable mutex name
//		osMutexPrioInherit|osMutexRecursive  ,    					// attr_bits
//		NULL,                // memory for control block   
//		0U                   // size for control block
//	},
//};
void RTE_MEM_Pool(RTE_MEM_Name_e mem_name,void *buf,uint32_t len)
{
	MemoryControlHandle[mem_name].work_mem = (uint8_t *) buf;
	MemoryControlHandle[mem_name].totalsize = len;
	RTE_MEM_Ent_t * full = (RTE_MEM_Ent_t *)MemoryControlHandle[mem_name].work_mem;
	full->header.used = 0;
	/*The total mem size id reduced by the first header and the close patterns */
	full->header.d_size = len - sizeof(RTE_MEM_Header_t);
#if RTE_USE_OS
	MemoryControlHandle[mem_name].MutexIDMEM = osMutexNew(NULL);
#endif
}
/*************************************************
*** Args:   mem_name ָ�����ڴ�����;
            requested_size ������ڴ��С��
*** Function: ��MEM��̬�����ڴ�
*************************************************/
void *RTE_MEM_Alloc(RTE_MEM_Name_e mem_name,uint32_t size)
{
	if(size == 0) {
		return &zero_mem;
	}
#if RTE_MEM_64BIT == 1
	/*Round the size up to 8*/
	if(size & 0x7) {
		size = size & (~0x7);
		size += 8;
	}
#else
	/*Round the size up to 4*/
	if(size & 0x3) {
		size = size & (~0x3);
		size += 4;
	}
#endif
  void * alloc = NULL;
	RTE_MEM_Ent_t * e = NULL;
	//Search for a appropriate entry
	do {
		//Get the next entry
		e = ent_get_next(mem_name,e);
		/*If there is next entry then try to allocate there*/
		if(e != NULL) {
			alloc = ent_alloc(e, size);
		}
		//End if there is not next entry OR the alloc. is successful
	} while(e != NULL && alloc == NULL);
	if(alloc == NULL) 
		RTE_LOG_WARN(MEM_STR,"Couldn't allocate memory");
	return alloc;
}
/*************************************************
*** Args:   mem_name BGet�ƶ���MEMNAME;
            requested_size ������ڴ��С��
*** Function: ��BGet�����ڴ棬�����
*************************************************/
void *RTE_MEM_Alloc0(RTE_MEM_Name_e mem_name,uint32_t size)
{
	void * alloc = NULL;
	alloc = RTE_MEM_Alloc(mem_name,size);
	if(alloc != NULL) memset(alloc, 0, size);
	else RTE_LOG_WARN(MEM_STR,"Couldn't allocate memory");
	return alloc;
}
/*************************************************
*** Args:   mem_name ָ�����ڴ�����;
            *buf ��ǰ������ڴ��ַ��
*** Function: �ͷ�����Ӧ������ڴ�
*************************************************/
void RTE_MEM_Free(RTE_MEM_Name_e mem_name,const void * data)
{
	if(data == &zero_mem) return;
	if(data == NULL) return;
	/*e points to the header*/
	RTE_MEM_Ent_t * e = (RTE_MEM_Ent_t *)((uint8_t *) data - sizeof(RTE_MEM_Header_t));
	e->header.used = 0;
#if RTE_MEM_AUTO_DEFRAG == 1
	/* Make a simple defrag.
	 * Join the following free entries after this*/
	RTE_MEM_Ent_t * e_next;
	e_next = ent_get_next(mem_name,e);
	while(e_next != NULL) {
		if(e_next->header.used == 0) {
			e->header.d_size += e_next->header.d_size + sizeof(e->header);
		} else {
			break;
		}
		e_next = ent_get_next(mem_name,e_next);
	}
#endif
}
/*************************************************
*** Args:   mem_name ָ�����ڴ�����;
						*buf ��ǰ������ڴ��ַ
            size ��������ڴ��С��
*** Function: �����µĴ�С���ڴ�ռ�
*************************************************/
void *RTE_MEM_Realloc(RTE_MEM_Name_e mem_name,void *data_p,uint32_t new_size)
{
	/*data_p could be previously freed pointer (in this case it is invalid)*/
	if(data_p != NULL) {
		RTE_MEM_Ent_t * e = (RTE_MEM_Ent_t *)((uint8_t *) data_p - sizeof(RTE_MEM_Header_t));
		if(e->header.used == 0) {
			data_p = NULL;
		}
	}
	uint32_t old_size = RTE_MEM_GetDataSize(data_p);
	if(old_size == new_size) return data_p;     /*Also avoid reallocating the same memory*/
	/* Only truncate the memory is possible
	 * If the 'old_size' was extended by a header size in 'ent_trunc' it avoids reallocating this same memory */
	if(new_size < old_size) {
		RTE_MEM_Ent_t * e = (RTE_MEM_Ent_t *)((uint8_t *) data_p - sizeof(RTE_MEM_Header_t));
		ent_trunc(e, new_size);
		return &e->first_data;
	}
	void * new_p;
	new_p = RTE_MEM_Alloc(mem_name,new_size);
	if(new_p != NULL && data_p != NULL) {
		/*Copy the old data to the new. Use the smaller size*/
		if(old_size != 0) {
			memcpy(new_p, data_p, MATH_MIN(new_size, old_size));
			RTE_MEM_Free(mem_name,data_p);
		}
	}
	if(new_p == NULL) RTE_LOG_WARN(MEM_STR,"Couldn't allocate memory");
	return new_p;
}
/*************************************************
*** Args:   mem_name ָ�����ڴ�����;
*** Function: ��Ƭ����
*************************************************/
void RTE_MEM_Defrag(RTE_MEM_Name_e mem_name)
{
	RTE_MEM_Ent_t * e_free;
	RTE_MEM_Ent_t * e_next;
	e_free = ent_get_next(mem_name,NULL);
	while(1) {
		/*Search the next free entry*/
		while(e_free != NULL) {
			if(e_free->header.used != 0) {
				e_free = ent_get_next(mem_name,e_free);
			} else {
				break;
			}
		}
		if(e_free == NULL) return;
		/*Joint the following free entries to the free*/
		e_next = ent_get_next(mem_name,e_free);
		while(e_next != NULL) {
			if(e_next->header.used == 0) {
				e_free->header.d_size += e_next->header.d_size + sizeof(e_next->header);
			} else {
				break;
			}
			e_next = ent_get_next(mem_name,e_next);
		}
		if(e_next == NULL) return;
		/*Continue from the lastly checked entry*/
		e_free = e_next;
	}
}
/*************************************************
*** Args:   mem_name ָ�����ڴ�����;
*** Function: ��Ϣͳ��
*************************************************/
void RTE_MEM_Monitor(RTE_MEM_Name_e mem_name,RTE_MEM_Monitor_t * mon_p)
{
	/*Init the data*/
	memset(mon_p, 0, sizeof(RTE_MEM_Monitor_t));
	RTE_MEM_Ent_t * e;
	e = NULL;
	e = ent_get_next(mem_name,e);
	while(e != NULL)  {
		if(e->header.used == 0) {
			mon_p->free_cnt++;
			mon_p->free_size += e->header.d_size;
			if(e->header.d_size > mon_p->free_biggest_size) {
				mon_p->free_biggest_size = e->header.d_size;
			}
		} else {
			mon_p->used_cnt++;
		}
		e = ent_get_next(mem_name,e);
	}
	mon_p->total_size = MemoryControlHandle[mem_name].totalsize;;
	mon_p->used_pct = 100 - (100U * mon_p->free_size) / mon_p->total_size;
	mon_p->frag_pct = (uint32_t)mon_p->free_biggest_size * 100U / mon_p->free_size;
	mon_p->frag_pct = 100 - mon_p->frag_pct;
}
/*************************************************
*** Args:   *buf ������ڴ��ַ;
*** Function: ��ȡ�������ڴ��ʵ�ʿռ��С
*************************************************/
uint32_t RTE_MEM_GetDataSize(const void * data)
{
	if(data == NULL) return 0;
	if(data == &zero_mem) return 0;
	RTE_MEM_Ent_t * e = (RTE_MEM_Ent_t *)((uint8_t *) data - sizeof(RTE_MEM_Header_t));
	return e->header.d_size;
}
/**
 * Give the next entry after 'act_e'
 * @param act_e pointer to an entry
 * @return pointer to an entry after 'act_e'
 */
static RTE_MEM_Ent_t * ent_get_next(RTE_MEM_Name_e mem_name,RTE_MEM_Ent_t * act_e)
{
#if RTE_USE_OS
	osMutexAcquire(MemoryControlHandle[mem_name].MutexIDMEM,osWaitForever);
#endif
	RTE_MEM_Ent_t * next_e = NULL;
	if(act_e == NULL) { /*NULL means: get the first entry*/
		next_e = (RTE_MEM_Ent_t *) MemoryControlHandle[mem_name].work_mem;
	} else { /*Get the next entry */
		uint8_t * data = &act_e->first_data;
		next_e = (RTE_MEM_Ent_t *)&data[act_e->header.d_size];
		if(&next_e->first_data >= &MemoryControlHandle[mem_name].work_mem[MemoryControlHandle[mem_name].totalsize]) next_e = NULL;
	}
#if RTE_USE_OS
	osMutexRelease(MemoryControlHandle[mem_name].MutexIDMEM);
#endif
	return next_e;
}
/**
 * Try to do the real allocation with a given size
 * @param e try to allocate to this entry
 * @param size size of the new memory in bytes
 * @return pointer to the allocated memory or NULL if not enough memory in the entry
 */
static void *ent_alloc(RTE_MEM_Ent_t * e, uint32_t size)
{
	void * alloc = NULL;
	/*If the memory is free and big enough then use it */
	if(e->header.used == 0 && e->header.d_size >= size) {
		/*Truncate the entry to the desired size */
		ent_trunc(e, size),
		e->header.used = 1;
		/*Save the allocated data*/
		alloc = &e->first_data;
	}
	return alloc;
}

/**
 * Truncate the data of entry to the given size
 * @param e Pointer to an entry
 * @param size new size in bytes
 */
static void ent_trunc(RTE_MEM_Ent_t * e, uint32_t size)
{
#if RTE_MEM_64BIT
	/*Round the size up to 8*/
	if(size & 0x7) {
		size = size & (~0x7);
		size += 8;
	}
#else
	/*Round the size up to 4*/
	if(size & 0x3) {
		size = size & (~0x3);
		size += 4;
	}
#endif
	/*Don't let empty space only for a header without data*/
	if(e->header.d_size == size + sizeof(RTE_MEM_Header_t)) {
		size = e->header.d_size;
	}
	/* Create the new entry after the current if there is space for it */
	if(e->header.d_size != size) {
		uint8_t * e_data = &e->first_data;
		RTE_MEM_Ent_t * after_new_e = (RTE_MEM_Ent_t *)&e_data[size];
		after_new_e->header.used = 0;
		after_new_e->header.d_size = e->header.d_size - size - sizeof(RTE_MEM_Header_t);
	}
	/* Set the new size for the original entry */
	e->header.d_size = size;
}
uint32_t RTE_MEM_MaxFree(RTE_MEM_Name_e mem_name)
{
	RTE_MEM_Monitor_t mon_infor = {0};
	RTE_MEM_Monitor(mem_name,&mon_infor);
	return mon_infor.free_biggest_size;
}
#endif
