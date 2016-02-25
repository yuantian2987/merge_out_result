// merge_out_and_mute.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"


#include "stdio.h"

typedef short                       INT16;
typedef int       			INT32;
typedef unsigned int               UINT32;
typedef  long long				INT64;
//typedef  __int64				INT64;
#define APP_INPUT_BUFF_SIZE  (0x1000)
#define APP_VOL_PCM_QINT_NUM (23+2-2)
#define APP_VOL_PCM_QDEC_NUM (5)
#define SAT_CNT_THRESHLOD   (100)
#define FADE_STEP           0x2

INT16 app_in_buff_left[APP_INPUT_BUFF_SIZE];//0x1000
INT16 app_in_buff_right[APP_INPUT_BUFF_SIZE];//0x1000
INT32 app_out_buff_left[APP_INPUT_BUFF_SIZE];//0x1000
INT32 app_out_buff_right[APP_INPUT_BUFF_SIZE];//0x1000
typedef enum {
    AIO_CH_SRC_MIDIA = 0, //media mode
    AIO_CH_SRC_SIF,       ///ATV mode
    AIO_CH_SRC_I2S,     //I2s mode
    AIO_CH_SRC_LINEIN0,
    AIO_CH_SRC_LINEIN1,
    APP_SCR_TYPE_NB            ///< Number of app of source type
} AIO_CH_SRC_ENUM;


typedef struct {
    INT32 *volume_table;
    INT16 volume_table_length;
    INT16 vol_index_r;
    INT16 vol_index_l;
    INT16 vol_std_index;
    INT32 vol_soft_vgain_r;
    INT32 vol_soft_vgain_l;
    INT32 sat_vol;
    UINT32 soft_mute_ctrl;
} SERV_APP_VOL_CTRL_T;

SERV_APP_VOL_CTRL_T app_vol_ctrl_inst;
SERV_APP_VOL_CTRL_T *g_app_vol_ctrl = &app_vol_ctrl_inst;
AIO_CH_SRC_ENUM source;
/*
.uni_fac_media=2836,
.uni_fac_hdmi=5992,
.uni_fac_sif=22926,
.uni_fac_line=1<<13
volume table
{479,483,485,485,487,502,496,506,508,510,517,544,553,599,623,682,768,883,1022,1200,1454,1587,1757,1953,2173,2387,2675,2950,3305,3625,4071,4312,4621,4972,5273,5656,6079,6437,6942,7486,8068,8320,8565,8954,9234,9652,9942,10243,10725,11061,11557,11739,12081,12255,12631,13008,13209,13605,13822,14231,14643,14865,15314,15542,16002,16461,16704,17197,17436,17946,18468,18741,19272,19558,20121,20692,20993,21595,21911,22545,23201,23337,24191,24553,25245,25975,26343,27096,27473,28247,29043,29870,30697,31548,32451,33395,34353,39185,39192,39194,39512}
*/
UINT32 vol_norm_factor;
static UINT32 ssa_attenuation_r=0;
static UINT32 ssa_attenuation_l=0;
void serv_app_merge_out_buf_data(INT16 *databufferIn_l,INT16 *databufferIn_r,INT16 len,INT32 * OutBuf_l,INT32 * OutBuf_r)
{
    int i;
    //UINT32 * OutBuf = app_cxt->out_stream.buff;
    //UINT32 tail = app_cxt->out_stream.tail;
    INT32 data,data_check;
    INT32 sat_cnt,reset_sat_cnt;
    UINT32 vol_soft_vgain_r,vol_soft_vgain_l,left_gain,right_gain;
    sat_cnt = 0;
    reset_sat_cnt=0;

    /*2 is to compensate treblebass's attenuation*/
    right_gain=vol_soft_vgain_r = (g_app_vol_ctrl->vol_soft_vgain_r * g_app_vol_ctrl->sat_vol*2) >> (12);
    left_gain=vol_soft_vgain_l = (g_app_vol_ctrl->vol_soft_vgain_l * g_app_vol_ctrl->sat_vol*2) >> (12);
    if(source == AIO_CH_SRC_SIF) {
        right_gain=vol_soft_vgain_r = (vol_soft_vgain_r*vol_norm_factor)>>(13);
        left_gain=vol_soft_vgain_l = (vol_soft_vgain_l*vol_norm_factor)>>(13);
    }
    for (i = 0; i<len; i++) {
        if(g_app_vol_ctrl->sat_vol < 0x1000) {
            data_check = (databufferIn_l[i]*g_app_vol_ctrl->vol_soft_vgain_r)>>2;
            if(data_check >((1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM))-1)) {
                reset_sat_cnt++;
            } else if(data_check< (-(1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM)))) {
                reset_sat_cnt++;
            }
        }
        vol_soft_vgain_r = right_gain;
        vol_soft_vgain_l = left_gain;
        //if((g_app_vol_ctrl->soft_mute_ctrl==0xffffffff)&&(soft_mute_gain<1024))
        if((g_app_vol_ctrl->soft_mute_ctrl >= 0x10000)&&(g_app_vol_ctrl->soft_mute_ctrl < 0x10400)) {
            g_app_vol_ctrl->soft_mute_ctrl += FADE_STEP;
            vol_soft_vgain_r = (vol_soft_vgain_r * (g_app_vol_ctrl->soft_mute_ctrl -0x10000))>>(10);
            vol_soft_vgain_l = (vol_soft_vgain_l * (g_app_vol_ctrl->soft_mute_ctrl -0x10000))>>(10);
        }
        if(g_app_vol_ctrl->soft_mute_ctrl) {
            data = (databufferIn_l[i]*vol_soft_vgain_r);
        } else {
            data = 0;
        }
        //data = (out_data[0][i]*vol_soft_vgain_r)&g_app_vol_ctrl->soft_mute_ctrl;//g_default_vol_table[g_vol_index_r];
        if(data >((1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM))-1)) {
            data = (1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM)) -1;
            sat_cnt++;
        } else if(data< (-(1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM)))) {
            data = -(1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM));
            sat_cnt++;
        }

        OutBuf_l[i] = ((data>>(APP_VOL_PCM_QDEC_NUM+ssa_attenuation_l))&(0xffffff));

        if(g_app_vol_ctrl->soft_mute_ctrl) {
            data = (databufferIn_r[i]*vol_soft_vgain_l);
        } else {
            data = 0;
        }
        //data = (out_data[1][i]*vol_soft_vgain_l)&g_app_vol_ctrl->soft_mute_ctrl;//g_default_vol_table[g_vol_index_l];
        if(data >((1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM))-1)) {
            data = (1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM)) -1;
            sat_cnt++;
        } else if(data < (-(1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM)))) {
            data = -(1<<(APP_VOL_PCM_QINT_NUM + APP_VOL_PCM_QDEC_NUM));
            sat_cnt++;
        }
        OutBuf_r[i] =(((data>>(APP_VOL_PCM_QDEC_NUM+ssa_attenuation_r))&(0xffffff)))|(1<<31);
    }
   // app_cxt->num_out_sample = pPassSrcM[0]->nOutputbuffersize;
    if(reset_sat_cnt <10) {
        g_app_vol_ctrl->sat_vol = 0x1000;
    } else {
        if(sat_cnt>=SAT_CNT_THRESHLOD) {
            g_app_vol_ctrl->sat_vol -= 8;
        }
    }

   // app_cxt->out_stream.tail = ((tail)&(app_cxt->out_stream.buff_size/4 - 1));
}

int main(int argc, char* argv[])
{
	FILE *fp_in,*fp_out,*fp_test;
	int cnt,n,i,data;
	fp_in  = fopen("in.bin","rb");//4//
	if (fp_in == NULL)
	{
		printf("aaaaab\n");
		return -1;
	}
	fp_out  = fopen("48_1K_32bit_out.txt","w");
	if (fp_out == NULL)
	{	
		printf("ddddd\n");
		return -2;
	}
	source=AIO_CH_SRC_LINEIN0;
	vol_norm_factor = 22926;
	g_app_vol_ctrl->sat_vol=0x1000;
	g_app_vol_ctrl->vol_soft_vgain_r =39512;
       g_app_vol_ctrl->vol_soft_vgain_l = 39512;
	g_app_vol_ctrl->soft_mute_ctrl = 0x10400;
	ssa_attenuation_r=0;
	ssa_attenuation_l=0;
	while(1)
	{
		n = fread(&app_in_buff_left,2,APP_INPUT_BUFF_SIZE,fp_in);
		//printf("read n = %d\n",n);
		if (n != APP_INPUT_BUFF_SIZE)
		{
			serv_app_merge_out_buf_data(app_in_buff_left,app_in_buff_right,n,app_out_buff_left,app_out_buff_right);
			for(i=0;i<n;i++)
			{
				data=app_out_buff_left[i];
				if(0x800000&data)
				{
					data = data |(0xFF800000);
				}
				fprintf(fp_out,"%08d\n",data);
			}
			printf("finished \n");
			break;
		}
		serv_app_merge_out_buf_data(app_in_buff_left,app_in_buff_right,n,app_out_buff_left,app_out_buff_right);
		for(i=0;i<n;i++)
		{
			data=app_out_buff_left[i];
			if(0x800000&data)
			{
				data = data |(0xFF800000);
			}
			fprintf(fp_out,"%08d\n",data);
		}
		//fwrite(app_out_buff_left,4,n,fp_out);
	}
	
	fclose(fp_in);
	fclose(fp_out);
	printf("Hello World!\n");
	return 0;
}
