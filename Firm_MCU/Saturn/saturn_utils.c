
#include "main.h"
#include "ff.h"
#include <stdlib.h>
#include "cdc.h"


/******************************************************************************/


u32 adler32(u8 *data, int len) 
{
	u32 a = 1, b = 0;
	int i;

	for(i=0; i<len; i++) {
		a = (a + data[i]) % 65521;
		b = (b + a) % 65521;
	}

	return (b<<16) | a;
}


/******************************************************************************/


// ��gameid�ֽ�Ϊid��ver
static char *gameid_split(char *gid)
{
	int len1, len2;
	char *ver, *p;

	len2 = 0;
	ver = strchr(gid, ' ');
	if(ver){
		*ver++ = 0;
		while(*ver==' ') ver++;
		p = strchr(ver, ' ');
		if(p) *p = 0;
		len2 = strlen(ver);
	}
	len1 = strlen(gid);
	if(len1+len2==16 && ver){
		strcpy(gid+len1, ver);
		ver = NULL;
	}

	return ver;
}


int gameid_match(char *gid1, char *gid2)
{
	char id1[24], id2[24];
	char *ver1, *ver2;

	strcpy(id1, gid1);
	strcpy(id2, gid2);

	ver1 = gameid_split(id1);
	ver2 = gameid_split(id2);

	if(strcmp(id1, id2)){
		return 0;
	}

	if(ver1 && ver1){
		if(strcmp(ver1, ver2)){
			return 0;
		}
	}else if(ver1==NULL && ver2==NULL){
	}else{
		return 0;
	}

	return 1;
}


/******************************************************************************/


static int max_depth;
static int *qstack = (int*)0x2400a000;
static int qtop;

#define qs_push(lp, rp)  ( qstack[qtop++] = ((lp)<<16) | (rp))

void qsort4(void *idata, int num, int (*cmp_func)(const void*, const void*))
{
	int *data = (int*)idata;
	int ilp, irp, lp, rp, flag;

	if(num<2)
		return;

	max_depth = 0;
	qtop = 0;
	qs_push(0, num-1);

	while(qtop){
		qtop -= 1;
		ilp = qstack[qtop]>>16;
		irp = qstack[qtop]&0xffff;

		lp = ilp;
		rp = irp;
		flag = -1;
		while(rp > lp){
			if(cmp_func(data+lp, data+rp)>0){
				int temp = data[rp];
				data[rp] = data[lp];
				data[lp] = temp;
				flag = -flag;
			}
			if(flag<0){
				rp -= 1;
			}else{
				lp += 1;
			}
		}
		if(ilp<lp-1) qs_push(ilp, lp-1);
		if(lp+1<irp) qs_push(lp+1, irp);
		if(max_depth<qtop){
			max_depth = qtop;
		}
	}
	//printk("qsort4: max_depth=%d\n", max_depth);
}


/******************************************************************************/


char *get_token(char **str_in)
{
	char *str, *start, match;

	if(str_in==NULL || *str_in==NULL)
		return NULL;
	str = *str_in;

	while(*str==' ' || *str=='\t') str++;

	if(*str=='"'){
		match = '"';
		start = str+1;
		str += 1;
	}else if(*str){
		match = ' ';
		start = str;
	}else{
		return NULL;
	}

	while(*str && *str!=match) str++;
	if(*str==match){
		*str = 0;
		*str_in = str+1;
	}else{
		*str_in = NULL;
	}

	return start;
}


char *get_line(u8 *buf, int *pos, int size)
{
	char *line = (char*)buf+*pos;
	
	if(*pos>=size)
		return NULL;

	while(*pos<size && (buf[*pos]!='\r' && buf[*pos]!='\n')) *pos = (*pos)+1;
	while(*pos<size && (buf[*pos]=='\r' || buf[*pos]=='\n')){
		buf[*pos] = 0;
		*pos = (*pos)+1;
	}

	return line;
}


/******************************************************************************/

static int sscfg_p = 0;

void ss_config_init(void)
{
	sscfg_p = 0;
	*(u32*)(SYSINFO_ADDR+0x0100) = 0;
}

void ss_config_put(u32 val)
{
	*(u32*)(SYSINFO_ADDR+0x0100+sscfg_p) = val;
	sscfg_p += 4;
	*(u32*)(SYSINFO_ADDR+0x0100+sscfg_p) = 0;
}

/******************************************************************************/


int config_exmem(char *lbuf)
{
	if(lbuf[1]!='M')
		return -1;

	if(lbuf[0]=='1'){
		ss_config_put(0x30000001);
	}else if(lbuf[0]=='4'){
		ss_config_put(0x30000004);
	}else{
		return -1;
	}

	printk("    exmem_%s\n", lbuf);
	return 0;
}


int config_wrmem(char *lbuf)
{
	char *p;

	int addr = strtoul(lbuf, &p, 16);

	p = strchr(lbuf, '=');
	if(p==NULL)
		return -1;
	p += 1;
	while(*p==' ') p += 1;

	int width = strlen(p);
	int val = strtoul(p, NULL, 16);
	addr = ((width/2)<<28) | (addr&0x0fffffff);
	ss_config_put(addr);
	ss_config_put(val);
	printk("    M_%08x=%x\n", addr, val);

	return 0;
}

static char catbuf[32];
int category_num = 0;

int config_category(char *lbuf)
{
	char *category = (char*)(SYSINFO_ADDR+0x0e80); // 610a3e80

	if(category_num<12){
		memcpy(category+category_num*32, catbuf, 31);
		category_num += 1;
		*(u8*)(SYSINFO_ADDR+0x0c) = category_num;
	}
	return 0;
}

char *get_category(int id)
{
	return (char*)(SYSINFO_ADDR + 0x0e80 + id*32); // 610a3e80
}


/******************************************************************************/


#define ARG_NON  0
#define ARG_HEX  1
#define ARG_DEC  2
#define ARG_STR  3

#define GA       0x0100


typedef struct {
	char *name;
	int type;
	void *action;
	void *action_ex;
}CFGARG;


CFGARG arg_list [] = {
	{"lang_id",      GA|ARG_DEC, &lang_id,},
	{"debug",        GA|ARG_HEX, &debug_flags,},
	{"log_mask",     GA|ARG_HEX, &log_mask,},
	{"auto_update",  GA|ARG_DEC, &auto_update,},
	{"category",     GA|ARG_STR, &catbuf, config_category},
	{"sector_delay",    ARG_DEC, &sector_delay,},
	{"play_delay",      ARG_DEC, &play_delay,},
	{"pend_delay",      ARG_DEC, &pend_delay,},
	{"exmem_",          ARG_NON, config_exmem,},
	{"M_",              ARG_NON, config_wrmem,},
	{"multi_disc",      ARG_STR, &mdisc_str,},
	{"sort_mode",    GA|ARG_DEC, &sort_mode,},

	{NULL},
};


int parse_config(char *fname, char *gameid)
{
	FIL fp;
	u8 *fbuf = (u8*)0x24002000;
	char *p = NULL;
	int retv;

	ss_config_init();

	retv = f_open(&fp, fname, FA_READ);
	if(retv){
		return -2;
	}

	u32 nread;
	retv = f_read(&fp, fbuf, f_size(&fp), &nread);
	f_close(&fp);
	if(retv){
		return -3;
	}

	printk("\nLoad config [%s] for [%s]\n", fname, (gameid==NULL)?"Global":gameid);

	int cpos = 0;
	int g_sec = 0;
	int in_sec = 0;
	char *lbuf;
	while((lbuf=get_line(fbuf, &cpos, nread))!=NULL){
_next_section:
		//����section: [xxxxxx]
		if(in_sec==0){
			if(lbuf[0]=='['){
				p = strrchr(lbuf, ']');
				if(p==NULL){
					return -4;
				}
				*p = 0;
				if(g_sec==0){
					// ��һ��sectionһ����global
					if(strcmp(lbuf+1, "global")){
						return -5;
					}
					g_sec = 1;
					in_sec = 1;
					printk("Global config:\n");
				}else if(gameid && gameid_match(lbuf+1, gameid)){
					// �ҵ�����game_idƥ���section
					in_sec = 1;
					printk("Game config:\n");
				}
			}
			continue;
		}else{
			if(lbuf[0]=='['){
				if(g_sec==1){
					g_sec = 2;
					in_sec = 0;
					if(gameid==NULL)
						break;
					goto _next_section;
				}else{
					break;
				}
			}
			CFGARG *arg = arg_list;
			while(arg->name){
				int nlen = strlen(arg->name);
				int type = arg->type;
				int global = (type&GA);
				type &= 0xff;
				if(global && (gameid || g_sec!=1) ){
					arg += 1;
					continue;
				}
				if(strncmp(lbuf, arg->name, nlen)==0){
					if(type==ARG_NON){
						int (*action)(char*) = arg->action;
						retv = action(lbuf+nlen);
					}else if(type==ARG_STR){
						p = strchr(lbuf+nlen, '"');
						if(p){
							char *st = p+1;
							p = strchr(st, '"');
							if(p){
								*p = 0;
								printk("    %s = \"%s\"\n", arg->name, st);
								strcpy((char*)(arg->action), st);
								retv = 0;
							}else{
								retv = -1;
							}
						}else{
							retv = -1;
						}
					}else{
						int base = (type==ARG_DEC)? 10 : 16;
						p = strchr(lbuf+nlen, '=');
						if(p){
							int value = strtoul(p+1, NULL, base);
							if(base==10){
								printk("    %s = %d\n", arg->name, value);
							}else{
								printk("    %s = %08x\n", arg->name, value);
							}
							*(int*)(arg->action) = value;
							retv = 0;
						}else{
							retv = -1;
						}
					}
					if(retv){
						printk("Invalid config line: {%s}\n", lbuf);
						led_event(LEDEV_SCFG_ERROR);
					}else{
						if(arg->action_ex){
							void (*action_ex)(void) = arg->action_ex;
							action_ex();
						}
					}
				}
				arg += 1;
			}

		}
	}
	printk("\n");

	return 0;
}

/******************************************************************************/

#define SSAVE_FILE "/SAROO/SS_SAVE.BIN"
#define SSAVE_ADDR (SAVINFO_ADDR+0x00000)

static FIL ss_fp;
static int ss_index;


#define SMEMS_FILE "/SAROO/SS_MEMS.BIN"
#define SMEMS_HDR  (SAVINFO_ADDR+0x10000)
#define SMEMS_BUF  (SAVINFO_ADDR+0x13000)

static FIL sm_fp;
static int sm_index0;
static int sm_index1;


int open_savefile(void)
{
	u32 rv;
	int retv;
	u8 *fbuf = (u8*)0x24002000;

	printk("Open SSAVE ...\n");
	ss_index = 0;

	retv = f_open(&ss_fp, SSAVE_FILE, FA_READ|FA_WRITE);
	if(retv==0){
		f_read(&ss_fp, fbuf, 0x40, &rv);
		if(strcmp((char*)fbuf, "Saroo Save File")==0){
			ss_index = 1;
			goto _open_mems;
		}
	}

	printk("SS_SAVE.BIN not found! Create now.\n");
	retv = f_open(&ss_fp, SSAVE_FILE, FA_READ|FA_WRITE|FA_CREATE_ALWAYS);
	if(retv){
		printk("Create SS_SAVE.BIN failed! %d\n", retv);
		return retv;
	}

	memset(fbuf, 0, 0x10000);
	strcpy((char*)fbuf, "Saroo Save File");
	f_write(&ss_fp, fbuf, 0x10000, &rv);
	f_sync(&ss_fp);

	ss_index = 1;

_open_mems:

	retv = f_open(&sm_fp, SMEMS_FILE, FA_READ|FA_WRITE);
	printk("Open SMEMS ... %d\n", retv);
	if(retv){
		printk("SS_MEMS.BIN not found! Create now.\n");
		retv = f_open(&sm_fp, SMEMS_FILE, FA_READ|FA_WRITE|FA_CREATE_ALWAYS);
		if(retv<0){
			printk("Create SS_MEMS.BIN failed! %d\n", retv);
			return retv;
		}
		f_lseek(&sm_fp, 0x800000);
		f_close(&sm_fp);
		f_open(&sm_fp, SMEMS_FILE, FA_READ|FA_WRITE);
	}

	f_read(&sm_fp, (u8*)SMEMS_HDR, 8192, &rv);
	sm_index0 = 0;
	sm_index1 = 0;

	printk("Done.\n");
	return 0;
}


int load_savefile(char *gameid)
{
	u32 rv;
	int retv, i;
	u8 *fbuf = (u8*)0x24002000;

	if(ss_index==0)
		return -1;

	f_lseek(&ss_fp, 0);
	f_read(&ss_fp, fbuf, 0x10000, &rv);

	for(i=1; i<4096; i++){
		ss_index = i;
		if(*(u32*)(fbuf+i*16)==0)
			break;
		if(strncmp((char*)fbuf+i*16, gameid, 8)==0){
			// found it
			printk("Found savefile at %08x\n", i*0x10000);
			f_lseek(&ss_fp, i*0x10000);
			f_read(&ss_fp, (u8*)SSAVE_ADDR, 0x10000, &rv);
			return 0;
		}
	}
	if(i==4096)
		return -1;

	// not found, create it.
	printk("Create savefile at %08x\n", i*0x10000);
	strcpy((char*)fbuf+i*16, gameid);
	// update index
	f_lseek(&ss_fp, i*16);
	f_write(&ss_fp, fbuf+i*16, 16, &rv);
	// write empty save
	memset((u8*)SSAVE_ADDR, 0, 0x10000);
	f_lseek(&ss_fp, i*0x10000);
	f_write(&ss_fp, (u8*)SSAVE_ADDR, 0x10000, &rv);

	fs_lock();
	f_sync(&ss_fp);
	fs_unlock();

	return 0;
}


int flush_savefile(void)
{
	u32 rv;

	if(ss_index==0)
		return -1;

	fs_lock();
	printk("Flush savefile at %08x\n", ss_index*0x10000);

	f_lseek(&ss_fp, ss_index*0x10000);
	f_write(&ss_fp, (u8*)SSAVE_ADDR, 0x10000, &rv);
	f_sync(&ss_fp);

	printk("Flush done.\n\n");
	fs_unlock();

	return 0;
}


/******************************************************************************/

int load_smems(int id)
{
	u32 rv;
	int is_hdr;

	is_hdr = id&0x8000;
	id &= 0x7fff;
	printk("Load SMEMS %04x\n", id);
	if(id==0)
		return 0;

	f_lseek(&sm_fp, id*1024);
	if(is_hdr){
		f_read(&sm_fp, (u8*)SMEMS_HDR+8192, 1024, &rv);
		sm_index0 = id;
	}else{
		f_read(&sm_fp, (u8*)SMEMS_BUF, 64*1024, &rv);
		sm_index1 = id;
	}

	return 0;
}


int flush_smems(int flag)
{
	u32 rv;

	int d = sm_index0-sm_index1;
	if(d>=0 && d<64){
		memcpy32((u8*)SMEMS_BUF+d*1024, (u8*)SMEMS_HDR+8192, 1024);
	}
	printk("\nflush_smems: flag=%02x s0=%04x s1=%04x d=%d\n", flag, sm_index0, sm_index1, d);
	fs_lock();

	if(flag&1){
		printk("Flush SMEMS header.\n");
		f_lseek(&sm_fp, 0);
		f_write(&sm_fp, (u8*)SMEMS_HDR, 8192, &rv);
	}
	if(flag&2){
		printk("Flush SMEMS block %04x.\n", sm_index0);
		if(d<0 || d>=64 || (flag&4)==0){
			f_lseek(&sm_fp, sm_index0*1024);
			f_write(&sm_fp, (u8*)SMEMS_HDR+8192, 1024, &rv);
		}
	}
	if(flag&4){
		printk("Flush SMEMS buffer %04x.\n", sm_index1);
		f_lseek(&sm_fp, sm_index1*1024);
		f_write(&sm_fp, (u8*)SMEMS_BUF, 64*1024, &rv);
	}

	f_sync(&sm_fp);

	printk("Flush done.\n\n");
	fs_unlock();

	return 0;
}


/******************************************************************************/


static FIL cover_fp;
static int cover_init = 0;
static int *ip_cache_table[12];
static int *ip_cache_ptr;

static int find_cover(char *gid, int fsum)
{
	u8 *cover_buf = (u8*)0x614f0000;
	int i;

	for(i=0; i<2048; i++){
		u8 *hdr = cover_buf+i*32;
		if(hdr[0]==0)
			break;

		if(strcmp((char*)hdr, gid))
			continue;
		if(*(u32*)(hdr+12)!=fsum)
			continue;

		return (int)hdr;
	}

	return -1;
}


static int last_chdr;

void load_cover(int index)
{
	int i, retv;
	u32 nread;

	if(index&0x8000){
		last_chdr = -1;
	}
	index &= 0x7fff;

	// ��ʼ��COVER����
	if(cover_init<0){
		// ��cover.bin
		return;
	}else if(cover_init==0){
		retv = f_open(&cover_fp, "/SAROO/cover.bin", FA_READ);
		if(retv!=FR_OK){
			// ��cover.binʧ��.
			cover_init = -1;
			return;
		}
		// ��cover.bin��ͷ����0x6140f000����
		f_read(&cover_fp, (u8*)0x614f0000, 0x10000, &nread);

		// ip_cache�������Ϸindex��cover��ӳ��.
		ip_cache_ptr = (int*)(0x614e0000);
		memset((u8*)0x614e0000, 0, 2560*4);
		// ÿ�������һ��ָ��ip_cache��ָ��.
		for(i=0; i<12; i++){
			ip_cache_table[i] = NULL;
		}

		memset((u8*)0x61400100, 0, 324*240);
		cover_init = 1;
		last_chdr = -1;
	}

	// ȡ�õ�ǰ����ip_cacheָ��.
	int *ip_cache = ip_cache_table[category_current];
	if(ip_cache==NULL){
		// ��ʼ����ǰ����ָ��
		ip_cache_table[category_current] = ip_cache_ptr;
		ip_cache = ip_cache_ptr;
		ip_cache_ptr += total_disc;
	}

	// ���coverͷ��ָ��
	int chdr = ip_cache[index];
	if(chdr==-1){
		// ����Ϸû��cover.
		printk("  No Cover!\n");
		goto _no_cover;
	}else if(chdr==0){
		// ��һ��ѡ�����Ϸ������cache
		u8 ipbuf[256];
		char gid[16];

		retv = get_disc_ip(index, ipbuf);
		if(retv<0){
			printk("  get_disc_ip failed!\n");
			ip_cache[index] = -1;
			return;
		}

		u8 *ip = ipbuf;
		if(strncmp((char*)ipbuf, "SEGA SEGASATURN ", 16)){
			ip = ipbuf+0x10;
		}

		memcpy(gid, ip+0x20, 16);
		char *p = strrchr(gid, 'V');
		if(p)
			*p = 0;
		p = strchr(gid, ' ');
		if(p)
			*p = 0;

		int fsum = adler32(ip+0x30, 32);
		chdr = (int)find_cover(gid, fsum);
		if(chdr<0){
			printk("  No Cover Found! {%12s} {%08x}\n", gid, fsum);
			ip_cache[index] = -1;
			goto _no_cover;
		}

		ip_cache[index] = chdr;
	}

	if(chdr==last_chdr)
		return;
	last_chdr = chdr;

	int w = *(u16*)(chdr+0x14);
	int h = *(u16*)(chdr+0x16);
	int offset = *(int*)(chdr+0x10);

	f_lseek(&cover_fp, offset);
	f_read(&cover_fp, (u8*)0x61400c00, w*h+0x0400, &nread);
	memcpy((u8*)0x61400100, (u8*)0x61400c00, 0x0400);

	*(u16*)(0x61400004) = w;
	*(u16*)(0x61400006) = h;
	*(u16*)(0x61400008) = 0;
	*(u16*)(0x6140000a) = 0;
	*(u16*)(0x6140000c) = 176;
	*(u16*)(0x6140000e) = (h>128)? 24 : 88;
	
	*(u8*)0x61400000 = 3;
	*(u8*)0x61400002 = 1;
	return;

_no_cover:
	last_chdr = -1;
	*(u16*)(0x61400004) = 128;
	*(u16*)(0x61400006) = 128;
	*(u16*)(0x61400008) = 0;
	*(u16*)(0x6140000a) = 0;
	*(u16*)(0x6140000c) = 176;
	*(u16*)(0x6140000e) = 88;

	memset((u8*)0x61401000, 0, 128*192);
	*(u8*)0x61400000 = 4;

	return;
}


/******************************************************************************/

