// Ray Tracer for 3D sound rendering
// Shipeng Xu 2013
// billhsu.x@gmail.com
// Shanghai University
#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include "glut.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <windows.h>
#include <string>
#include <sstream>
#include "Scene/Scene.h"
#include "Scene/Primitive.h"
#include "Scene/Ray.h"
#include "common.h"
#include "time.h"
#include "Audio/hrtf.h"
#include "Audio/wav.h"
#include "Math/Matrices.h"
#include "SerialPort/SerialPort.h"
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <time.h>
#include <stdlib.h>
#include <GL/glui.h>

//#pragma comment( lib, "glaux.lib")		// GLaux连接库

/** glui params **/
GLUI *glui, *glui2;
GLUI_Spinner    *light0_spinner, *light1_spinner;
GLUI_RadioGroup *radio;
GLUI_Panel      *obj_panel;
int main_window;
int audio_type=0;
int scene_type=0;
int source_type=0;
int sound_type=0;
int ray_num=0;
int order=0;
float radius=0.0f;
float source_x=0.0f;
float source_y=0.0f;
float source_z=0.0f;
float receiver_x=0.0f;
float receiver_y=0.0f;
float receiver_z=0.0f;
float xy_aspect;
#define PLAY_ID 300
/** end **/



#define REFLECTION_NUM 4
#define RAY_NUMBER 50
#define ea_num 10000
#define dj_num 100
#define mc_num 70
#define ROOM_SIZE 6.0f
#define MAX_DIST 20000.0f
#define RADIUS 0.7f
#define r_cof 0.99
#define d_cof 0.20
#define F1_PATH "D:\\anze_data.txt"
#define F2_PATH "D:\\lont_data.txt"
#define NUM_DEMOS 5

//场景选择
RayTracer::Scene scene;
int currentDemoIndex=0;
std::string scene_file1[NUM_DEMOS]={"Res/box.obj","Res/two_box.obj","Res/Scene_GSound.obj","Res/lab.obj","Res/Simple Outdoor.obj"};

static int num_intersect=0;
static int num_test=0;

boost::mutex mutex;

bool keys[256];
bool specialKeys[256];

void initCalc();
hrtf mhrtf("data\\hrtf");
float rot_x=0.0f,rot_y=0.0f;
DWORD startTime;
std::vector<RayTracer::Ray> rayListTmp;

std::string music_file;
std::string scene_file;
char file_s[20]={0};

wav mWav;
float *response_r;
float *response_l;

float *rir;

float const time441k = 22.675736961451247165532879818594f;
struct respond
{
    RayTracer::vector3 direction;
    float strength;
	float totalDistance;
    int time;
	int refl_num;
	std::vector<int> path;
    bool operator<(const respond &rhs) const { return strength > rhs.strength; }
};
std::vector<respond> respondList;
std::vector<respond> ImpulseResponse;
std::vector<respond> ImpulseResponseTmp;

short* music;
//Compute ray tracing by ray simulation 
RayTracer::vector3 origin = RayTracer::vector3(2.0f,1.0f,1.2f);
RayTracer::vector3 listener  = RayTracer::vector3(1.0f,1.5f,1.0f);
float rot_z = 0.0f;

bool start_flag = false;
int start_match_pos = 0;
int recv_cnt = 0;

BYTE recv_data[26]={0};
BYTE start_mark[]={0xa5,0x5a,0x12,0xa1};
int imu_result[14]={0};
float yaw=0.0f,pitch=0.0f,roll=0.0f;

int band[8]={0,125,500,1000,2000,4000,22050};
int humidity=15;
// float m_air=5.5E-4*(50/humidity)*(F_abs.*1E-3).^1.7;
SerialPort serial;
struct convData
{
    short* buffer;
    short* buffer_last;
    short* music;
    int dataSize;
    int kernelSize;
    float* response_l;
    float* response_r;
};
const int divide = 2;
short* volatile buffer;
short* volatile buffer_old;
short* volatile buffer_last;
DWORD WINAPI convThread(LPVOID data) {
    boost::lock_guard<boost::mutex> m_csLock(mutex);
    hrtf::convAudio(((convData*)data)->buffer,((convData*)data)->buffer_last,
        ((convData*)data)->music,((convData*)data)->dataSize,
        ((convData*)data)->kernelSize,((convData*)data)->response_l,
        ((convData*)data)->response_r);
    return 0;
}
DWORD WINAPI playwavThread(LPVOID dataSize) {
    mWav.playWave(buffer_old,*(int*)dataSize*4);
    return 0;
}
DWORD WINAPI waveThread(LPVOID data) {
    while( true )
    {
        initCalc();
    }
    return 0;
}

std::vector<respond> fuzhi(std::vector<respond> respondList)
{
	std::vector<respond> tmp;
	for(int i=0;i<respondList.size();i++)
	{
		tmp.push_back(respondList[i]);
	}
	return tmp;
}

bool equalpath(std::vector<int> path1,std::vector<int> path2)
{
	bool flag;
	if(path1.size()!=path2.size())
		 flag=false;
	int length=min(path1.size(),path2.size());
	for(int i=0;i<length;i++)
	{
		if(path1[i]==path2[i])
			flag=true;
		else
		{
			flag=false;
			break;
		}
	}
	return flag;
}

bool equalRespondPath(std::vector<respond> respondList)
{
	bool result=false;
	for(int i=0;i<respondList.size()-1;i++)
	{
		for(int j=i+1;j<respondList.size();j++)
		{
			bool flag;
			flag=equalpath(respondList[i].path,respondList[j].path);
			if(flag)
			{
				result=true;//表示有相同的路径，则需要再次进行处理
				break;
			}
			//else
			//{
			//	result=false;
			//}		
		}
	}
	return result;
}


void setCurrentDemo( int demoIndex )
{
	if ( demoIndex >= NUM_DEMOS )
	{
		printf("\nInvalid demo index: %d",demoIndex);
		return;
	}
	currentDemoIndex = demoIndex;
}

void initializeScene()
{
	
	scene.loadObj(scene_file1[currentDemoIndex]);
	
	
}

void initCalc()
{
    

	
    DWORD start1=GetTickCount();
	long filelen;
    mWav.setMutex(&mutex);
    music = mWav.readWavFileData("Res/tada.wav",filelen);
	
	//char file_c[30]={0};
	//music = mWav.readWavFileData(strcpy(file_c,music_file.c_str()),filelen);
	
    rayListTmp.clear();
    respondList.clear();
	ImpulseResponse.clear();
    response_l=new float[1024];
    response_r=new float[1024];
    memset(response_l,0,1024*sizeof(float));
    memset(response_r,0,1024*sizeof(float));

    mWav.openDevice();
    RayTracer::Scene scene;

	//read  the azimuth information
	float anze_data[ea_num]={0};
	float lont_data[ea_num]={0};
	
	FILE *fp1,*fp2;
	fp1=fopen(F1_PATH,"r");
	fp2=fopen(F2_PATH,"r");
	
	if(fp1==NULL)
		printf("NO anze_data.txt\n");
	if(fp2==NULL)
		printf("NO lont_data.txt\n");
	
	int i=0;
	while(!feof(fp1)&&!feof(fp2)&&i!=ea_num)
	{
		fscanf(fp1,"%f",&anze_data[i]);
		fscanf(fp2,"%f",&lont_data[i]);
		
		i++;
	}
	fclose(fp1);
	fclose(fp2);
	
	//三种不同的声源分布方法
	//1、equal-area
	/*for(int i=0;i<ea_num;i++)
		{
				RayTracer::vector3 dir;
				dir.x = cosf(lont_data[i])*sinf(anze_data[i]);
				dir.y = sinf(lont_data[i])*sinf(anze_data[i]);
				dir.z = cosf(anze_data[i]);
				RayTracer::Ray ray(origin, dir);
				ray.strength=1.0f;
				ray.microseconds=0;
				ray.totalDist = 0.0f;
				ray.active=true;
				ray.reflnum=0;
				rayListTmp.push_back(ray);
		}*/
	
	//2、equal-angle
  /* for(int theta=0;theta<dj_num;++theta)//水平角：0<theat<360
    {
        for(int phi=-dj_num/2;phi<dj_num/2;++phi)//仰角：-90<phi<90
        {
			float rdn_num=0.0f;
			srand((float)time(NULL));
			rdn_num=-0.5f+(float)(1.0*rand()/RAND_MAX);
            RayTracer::vector3 dir;
            dir.x = cosf((1.0f+rdn_num)*2.0f*PI/dj_num*theta)*sinf((1.0f+rdn_num)*PI/dj_num*phi);
            dir.y = sinf((1.0f+rdn_num)*2.0f*PI/dj_num*theta)*sinf((1.0f+rdn_num)*PI/dj_num*phi);
            dir.z = cosf((1.0f+rdn_num)*PI/dj_num*phi);
            RayTracer::Ray ray(origin, dir);
            ray.strength=1.0f;
            ray.microseconds=0;
            ray.totalDist = 0.0f;
			ray.reflnum=0;
            ray.active=true;
            rayListTmp.push_back(ray);
        }
    }*/
	
   //3、Monte Carlo
  float mc_rand[2]={0.0f};
   srand((float)time(NULL));
   
   for(int i=0;i<mc_num;i++)
   {
	for(int j=0;j<mc_num;j++)
	{
		for(int k=0;k<2;k++)
	    mc_rand[k]=(float)(1.0*rand()/RAND_MAX);	   
		float r1=mc_rand[0];
		float r2=mc_rand[1];
		float theta=2.0*acos(sqrt(1.0-r1));
		float phi=2.0*PI*r2;
		RayTracer::vector3 dir;
				dir.x = cosf(phi)*sinf(theta);
				dir.y = sinf(phi)*sinf(theta);
				dir.z = cosf(theta);
				RayTracer::Ray ray(origin, dir);
				ray.strength=1.0f;
				ray.microseconds=0;
				ray.totalDist = 0.0f;
				ray.active=true;
				ray.reflnum=0;
				rayListTmp.push_back(ray);
	}
   }
  

    //std::cout<<"Sound position:"<<origin<<" Listener position: "<<listener<<std::endl;
    startTime = GetTickCount();
    RayTracer::Primitive p= RayTracer::Primitive(RayTracer::vector3(1,-1.5,-1.5),
        RayTracer::vector3(1,-1.5,1.5),
        RayTracer::vector3(0,1,0));
    //scene.primList.push_back(p);

	/*if (scene_type==0)
	currentDemoIndex=0;
	else if(scene_type==1)
	currentDemoIndex=1;
	else
	currentDemoIndex=2;*/
	scene.loadObj(scene_file1[currentDemoIndex]);
	//scene.loadObj(strcpy(file_s,scene_file.c_str()));
    //scene.loadObj("Res/box.obj");
    //Compute delays
    int active_rays = rayListTmp.size();
    clock_t start, finish;
    start = clock();
    while(active_rays>0)
    {
        for(unsigned int i=0;i<rayListTmp.size();++i)
        {
            if(rayListTmp[i].active==false) continue;
          // static int reflnum=0;
		//	rayListTmp[i].reflnum=0;
            float dist_=MAX_DIST;
			num_test++;
            int which = scene.intersect(rayListTmp[i],dist_);
            if(which!=MISS)//判断是否跟场景相交
            {
				num_intersect++;
				rayListTmp[i].distList.push_back(dist_);
				if(rayListTmp[i].reflnum>REFLECTION_NUM||rayListTmp[i].totalDist>=MAX_DIST || rayListTmp[i].strength<=0.0)
                {
                    rayListTmp[i].active=false;
                    active_rays--;
                    continue;
                }
            }

            RayTracer::vector3 L = listener - rayListTmp[i].GetOrigin();
            float Tca = DOT(L,rayListTmp[i].GetDirection());
            float d2;
            float dist_to_listener = 1000.0f;
            if(Tca>0)
            {
                d2 = DOT(L,L) - Tca*Tca;
                if(d2<RADIUS*RADIUS)//判断是否到达听者,半径为0.3
                {
                    
					float Thc = sqrt(RADIUS*RADIUS-d2);
                    dist_to_listener = Tca-Thc;
                }
            }
            if(dist_>dist_to_listener)//如果到达，对其距离进行叠加，并将此光线进行停止发射
            {
				
					rayListTmp[i].totalDist+=(dist_to_listener);
					rayListTmp[i].path.push_back(0);
					//rayListTmp[i].strength-=dist_to_listener/10.0f;
					//if(rayListTmp[i].strength<=0.0f)rayListTmp[i].strength=0.0f;
					//rayListTmp[i].active=false;
					//active_rays--;

					//std::cout<<rayListTmp[i].totalDist/0.000340f<<"μs, "<<rayListTmp[i].strength<<" "<<
					//    rayListTmp[i].GetDirection()<<std::endl;
					//std::cout<<rayListTmp[i].GetDirection()<<" ";
					//mhrtf.getHRTF(rayListTmp[i].GetDirection());
					
					respond respnd;//将到达的光线加入RIR的计算
					for(int ii=0;ii<rayListTmp[i].path.size();ii++)
					{
						respnd.path.push_back(rayListTmp[i].path[ii]);
					}
					respnd.strength=rayListTmp[i].strength;
					respnd.totalDistance=rayListTmp[i].totalDist;
					// respnd.time=(int)((rayListTmp[i].totalDist/0.000340f)/time441k);
					respnd.time=(int)rayListTmp[i].totalDist*44100.0f/340.0f;
					respnd.refl_num = rayListTmp[i].reflnum;
					respnd.direction = rayListTmp[i].GetDirection();
					respondList.push_back(respnd);
				//}
				
            }
            if(which != MISS)//没有到达听者，重新计算其反射方向，并有能量的衰减
            {
				rayListTmp[i].path.push_back(which);
				rayListTmp[i].reflnum++;
                rayListTmp[i].totalDist+=dist_;
               // rayListTmp[i].strength-=dist_/10.0f;//这里的距离衰减暂时不考虑
                //rayListTmp[i].strength-=0.01f;//相当于吸声系数为0.01
				// float ss=pow(0.99f,reflnum);

				
				//两种物理现象的模拟
				//1、diffuse model
				//float diffuse_rand[3]={0.0f};
				//srand((float)time(NULL));
				//for(int k=0;k<3;k++)
				//diffuse_rand[k]=(float)(1.0*rand()/RAND_MAX);
				//float R1=diffuse_rand[0];
				//float R2=diffuse_rand[1];
				//float R3=diffuse_rand[2];

				//if(R1>=0&&R1<d_cof)
				//{
				//	rayListTmp[i].strength*=r_cof*d_cof;
				//	RayTracer::vector3 end=rayListTmp[i].GetOrigin()+rayListTmp[i].GetDirection()*(dist_*0.999f);
				//	float c1=sqrt(R2)*cos(2*PI*R3);
				//	float c2=sqrt(R2)*sin(2*PI*R3);
				//	float c3=sqrt(1-R2);
				//	RayTracer::vector3 dir=RayTracer::vector3(c1,c2,c3);
				//	/*RayTracer::vector3 dir=-2*DOT(scene.primList[which].GetNormal(),rayListTmp[i].GetDirection())
    //                *scene.primList[which].GetNormal()+rayListTmp[i].GetDirection();*/
				//	 dir.Normalize();
				//	 rayListTmp[i].SetDirection(dir);
				//	 rayListTmp[i].SetOrigin(end);
				//}
				//else if(R1>=d_cof&&R1<d_cof+r_cof)
				//{
				//	rayListTmp[i].strength*=r_cof;
				//	RayTracer::vector3 end=rayListTmp[i].GetOrigin()+rayListTmp[i].GetDirection()*(dist_*0.999f);
				//	RayTracer::vector3 dir=-2*DOT(scene.primList[which].GetNormal(),rayListTmp[i].GetDirection())
    //                *scene.primList[which].GetNormal()+rayListTmp[i].GetDirection();
				//	 dir.Normalize();
				//	 rayListTmp[i].SetDirection(dir);
				//	 rayListTmp[i].SetOrigin(end);
				//}
				//else
				//{
				//	rayListTmp[i].active=false;
				//}

				//2、reflection model
				rayListTmp[i].strength*=0.99f;
                RayTracer::vector3 end=rayListTmp[i].GetOrigin()+rayListTmp[i].GetDirection()*(dist_*0.999f);
                RayTracer::vector3 dir=-2*DOT(scene.primList[which].GetNormal(),rayListTmp[i].GetDirection())
                    *scene.primList[which].GetNormal()+rayListTmp[i].GetDirection();
                dir.Normalize();
                rayListTmp[i].SetDirection(dir);
                rayListTmp[i].SetOrigin(end);
				
            }
            
        }
    }

	std::cout<<"num_intersect="<<num_intersect<<std::endl;
	std::cout<<"num_test="<<num_test<<std::endl;
	
DWORD end1=GetTickCount();
float time_elapse=(float)(end1-start1)/1000.0f;
//std::cout<<start<<std::endl;
//std::cout<<end<<std::endl;
std::cout<<"audio time:";
std::cout<<time_elapse<<std::endl;

//去掉重复的光线
bool result=true;
std::vector<respond> tmp;
tmp=fuzhi(respondList);
int num_index=0;
while(result)
{
		//将num_index前面的数值保存起来，即已经排好的那些值
		for(int i=0;i<num_index;i++)
			ImpulseResponseTmp.push_back(tmp[i]);
		//int j=0;//j表示被比较的那个数
		int repeat_cnt=0;
		for(int i=num_index;i<tmp.size();i++)//消去跟j相同的数值
		{				
				bool flag=equalpath(tmp[i].path,tmp[num_index].path);
				if(flag)
				{
					repeat_cnt++;
					if(repeat_cnt<2)
					{
						ImpulseResponseTmp.push_back(tmp[i]);
					}			
				}
				else
				{
					ImpulseResponseTmp.push_back(tmp[i]);	
				}		
		}
		result=equalRespondPath(ImpulseResponseTmp);
		tmp.clear();
		tmp=fuzhi(ImpulseResponseTmp);
		if(!result)
		ImpulseResponse=fuzhi(ImpulseResponseTmp);
		ImpulseResponseTmp.clear();
		num_index=num_index+1;
}

	std::ofstream out1("data/rir.txt");
    //out1<<"a =[";
	for(int i=0;i<ImpulseResponse.size();++i) 
    {
		out1<<ImpulseResponse[i].strength<<std::endl;
    }
    //out1<<"]"<<std::endl;
    
	/*std::ofstream out11("data/rir_34.txt");
    //out1<<"a =[";
	for(int i=0;i<ImpulseResponse.size();++i) 
    {
		if(ImpulseResponse[i].path.size()>REFLECTION_NUM)
		out11<<ImpulseResponse[i].strength<<std::endl;
    }*/

	std::ofstream out2("data/distance.txt");
  //  out2<<"d =[";
	for(int i=0;i<ImpulseResponse.size();++i) 
    {
		out2<<ImpulseResponse[i].totalDistance<<std::endl;
    }
 //   out2<<"]"<<std::endl;
    
	/*std::ofstream out22("data/distance_34.txt");
	for(int i=0;i<ImpulseResponse.size();++i) 
    {
		if(ImpulseResponse[i].path.size()>REFLECTION_NUM)
		out22<<ImpulseResponse[i].totalDistance<<std::endl;
    }*/
 

	std::ofstream out3("data/path.txt");
	for(int i=0;i<respondList.size();i++)
	{
		for(int j=0;j<respondList[i].path.size();j++)
		{
			out3<<respondList[i].path[j]<<" ";
		}
		out3<<std::endl;
	}

	std::ofstream out4("data/new_path.txt");
	for(int i=0;i<ImpulseResponse.size();i++)
	{
		for(int j=0;j<ImpulseResponse[i].path.size();j++)
		{
			out4<<ImpulseResponse[i].path[j]<<" ";
		}
		out4<<std::endl;
	}

	/*std::ofstream out5("data/rir_4order.txt");
	for(int i=0;i<ImpulseResponse.size();i++)
	{
		for(int j=0;j<ImpulseResponse[i].path.size();j++)
		{
			if(ImpulseResponse[i].path[j].size()>REFLECTION_NUM)
			out5<<ImpulseResponse[i].path[j]<<" ";
		}
		out5<<std::endl;
	}*/

    /*std::ifstream in("data/hrtf_0_0_r.txt");
    float hrtf[128]={0.0f};
    for(int i=0;i<128;++i) in>>hrtf[i];*/

   // printf("stage 2\n");
    float* hrtf; 
    hrtf::ir_both ir;
    std::sort(respondList.begin(), respondList.end());
    for(unsigned int i=0;i<respondList.size();++i)
    {
        Matrix4 m1;
        m1.rotateY(rot_z);
        Vector3 newDir;
        newDir.x=respondList[i].direction.z;
        newDir.y=respondList[i].direction.y;
        newDir.z=respondList[i].direction.x;
        newDir=m1*newDir;
        respondList[i].direction.x=newDir.x;
        respondList[i].direction.y=newDir.y;
        respondList[i].direction.z=newDir.z;
        ir = mhrtf.getHRTF(respondList[i].direction);
        hrtf = ir.ir_l;
        for(int j=0;j<128;++j)
        {
            if(respondList[i].time+j<1024)response_l[respondList[i].time+j]+=(hrtf[j]*respondList[i].strength);
        }
        hrtf = ir.ir_r;
        for(int j=0;j<128;++j)
        {
            if(respondList[i].time+j<1024)response_r[respondList[i].time+j]+=(hrtf[j]*respondList[i].strength);
        }
        //printf("%d/%d\n",i,respondList.size());
    }

    
    int divide=1;
    int kernelSize=1024;
    int dataSize = 71296/divide;


    buffer = new short[(dataSize+kernelSize)*2];
    buffer_old = new short[(dataSize+kernelSize)*2];
    buffer_last = new short [kernelSize*2];
    hrtf::convAudio(buffer_old,buffer_last,music,dataSize,kernelSize,response_l,response_r,true);

    convData *data = (convData*) malloc(sizeof(convData));
    data->dataSize=dataSize;
    data->kernelSize=kernelSize;
    data->response_l=response_l;
    data->response_r=response_r;
    
    HANDLE convHandle,playwavHandle;
    short* tmpbuf;


    playwavHandle = CreateThread(NULL,0,playwavThread,(LPVOID)&dataSize,0,NULL);
    
    for(int i=1;i<divide;++i)
    {
        data->buffer=buffer;
        data->buffer_last = buffer_last;
        data->music=&music[i*dataSize*2];
        convHandle = CreateThread(NULL,0,convThread,(LPVOID)data,0,NULL);
        //printf("%d CreateThread\n",i);
        
        
        
        WaitForSingleObject(playwavHandle,INFINITE);

        {
            boost::lock_guard<boost::mutex> m_csLock(mutex);
            tmpbuf = buffer_old;
            buffer_old = buffer;
            buffer = tmpbuf;
        }
        
        playwavHandle = CreateThread(NULL,0,playwavThread,(LPVOID)&dataSize,0,NULL);
        //printf("-----------\n");
        //system("pause");
    }
    WaitForSingleObject(playwavHandle,INFINITE);
    free(buffer_old);
    buffer_old = NULL;
    free(buffer);
    buffer = NULL;
    free(music);
    mWav.unprepWave();
    finish = clock();
    mWav.closeDevice();
    double duration = (double)(finish - start) / CLOCKS_PER_SEC;
    //printf( "%f seconds\n", duration );
	std::cout<<"BRIR:"<<duration<<std::endl;

    std::ofstream out("data/response.txt");
    out<<"a =[";
    for(int i=0;i<1024;++i) 
    {
        out<<response_l[i]<<" "<<response_r[i]<<"; ";
    }
    out<<"]"<<std::endl;
    

}

std::vector<RayTracer::Ray> rayList;

void control_cb(int control)
{
	if(control==PLAY_ID)
		glui->enable();
}

void keyinput(unsigned char key, int x, int y)
{
	long filelen;
    Matrix4 m1,m2;
    m1.rotateY(rot_z);
    Vector3 vx=Vector3(0.1,0,0);
    vx=m1*vx;
    m2.rotateY(rot_z+90);
    Vector3 vz=Vector3(0.1,0,0);
    vz=m2*vz;
    HANDLE waveHandle;
    switch(key)
    {
    case 'w':
        listener.x+=vx.x;
        listener.z+=vx.z;
        std::cout<<listener<<std::endl;
        break;
    case 's':
        listener.x-=vx.x;
        listener.z-=vx.z;
        std::cout<<listener<<std::endl;
        break;
    case 'a':
        listener.x+=vz.x;
        listener.z+=vz.z;
        std::cout<<listener<<std::endl;
        break;
    case 'd':
        listener.x-=vz.x;
        listener.z-=vz.z;
        std::cout<<listener<<std::endl;
        break;
    case 'j':
        rot_z+=1.5f;
        break;
    case 'k':
        rot_z-=1.5f;
        break;
    case 'p':
        //waveHandle = CreateThread(NULL,0,waveThread,(LPVOID)NULL,0,NULL);
        break;
	/*case '1':
		 music=mWav.readWavFileData("Res/bird.wav",filelen);
		 break;
	case '2':
		 music=mWav.readWavFileData("Res/baby.wav",filelen);
		 break;
	case '3':
		 music=mWav.readWavFileData("Res/woman.wav",filelen);
		 break;
	case '4':
		 music=mWav.readWavFileData("Res/tada.wav",filelen);
		 break;	 */
    case 27:
        exit(0);
        break;
    }
}


/*void handleInput()
{
	long filelen;
	if(keys['1'])
		music=mWav.readWavFileData("Res/bird.wav",filelen);
	if(keys['2'])
		music=mWav.readWavFileData("Res/baby.wav",filelen);
	if(keys['3'])
		 music=mWav.readWavFileData("Res/woman.wav",filelen);
	if(keys['4'])
		music=mWav.readWavFileData("Res/tada.wav",filelen);
}*/

void init(void)
{
    glClearColor (0.0, 0.0, 0.0, 0.0);
    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);

    glShadeModel (GL_SMOOTH);

    for(int theta=0;theta<dj_num;++theta)
    {
        for(int phi=-dj_num/2;phi<dj_num/2;++phi)
        {
            RayTracer::vector3 dir;
            dir.x = cosf(2.0f*PI/dj_num*theta)*sinf(2.0f*PI/dj_num*phi);
            dir.y = sinf(2.0f*PI/dj_num*theta)*sinf(2.0f*PI/dj_num*phi);
            dir.z = cosf(2.0f*PI/dj_num*phi);
            RayTracer::Ray ray(origin, dir);
            ray.strength=1.0f;
            ray.microseconds=0;
            ray.totalDist = 0.0f;
            ray.active=true;
            rayList.push_back(ray);
        }
    }
    /*RayTracer::Ray ray(RayTracer::vector3(1.5f,0.0f,0.0f), RayTracer::vector3(-1.0f,0.0f,0.0f));
    ray.strength=1.0f;
    ray.milliseconds=0;
    ray.active=true;
    ray.GetDirection().Normalize();
    rayList.push_back(ray);*/
    glutGet(GLUT_ELAPSED_TIME);
    //std::cout<<"Sound position:(1.5,0,0)  Listener position: (-1.5,0,0)"<<std::endl;
    /*startTime = GetTickCount();
    RayTracer::Primitive p= RayTracer::Primitive(RayTracer::vector3(1,-1.5,-1.5),
        RayTracer::vector3(1,-1.5,1.5),
        RayTracer::vector3(0,1,0));*/

    scene.primList.clear();
    //scene.loadObj(strcpy(file_s,scene_file.c_str()));
    scene.loadObj(scene_file1[currentDemoIndex]);

}
//Compute ray tracing by per frame ray simulation 

//frame speed function
double CalFrequency()
{
	static int count;
	static double save;
	static clock_t last, current;
	double timegap;

	++count;
	if( count <= 50 )
	return save;
	count = 0;
	last = current;
	current = clock();
	timegap = (current-last)/(double)CLK_TCK;
	save = 50.0/timegap;
	return save;
}

void display(void)
{
	
	double FPS= CalFrequency();
	std::cout<<"FPS:"<<FPS<<std::endl;
    glClearColor (0.2f, 0.2f, 0.2f, 0.0f);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glColor3f (1.0, 1.0, 1.0);
    glLoadIdentity ();             /* clear the matrix */
    /* viewing transformation  */
    gluLookAt (3.0, 4.0, -5.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    glRotatef(rot_x,1.0f,0.0f,0.0f);
    glRotatef(rot_y,0.0f,1.0f,0.0f);
    
    glPushMatrix();
    
    glTranslatef (listener.x, listener.y, listener.z);
    glRotatef(rot_z,0.0f,1.0f,0.0f);
    glBegin(GL_LINES);
    glColor3f(1.0, 1.0, 0.0);
    glVertex3f(0,0,0);
    glVertex3f(1,0,0);
    glEnd();
    glColor3f(0.7, 0.5, 0.5);
    glutWireSphere (0.5f, 10.0f, 10.0f);
    glPopMatrix();

    glLineWidth(2.0); 
    glColor3f(1.0, 0.0, 0.0);

    int active_cnt=0;
    glColor3f(1.0, 0.0, 1.0);

    for(unsigned int i=0;i<rayList.size();++i)
    {
        if(!rayList[i].active) continue;
        active_cnt++;
        glBegin(GL_LINES);
        RayTracer::vector3 end,dir;
        end=rayList[i].GetOrigin()+rayList[i].GetDirection()*0.1f;
        float dist_=0.1f;
        int which = scene.intersect(rayList[i],dist_);
        rayList[i].microseconds++;
        if(which!=MISS)
        {
            end=rayList[i].GetOrigin()+rayList[i].GetDirection()*dist_;
            RayTracer::vector3 dir=-2*DOT(scene.primList[which].GetNormal(),rayList[i].GetDirection())
                *scene.primList[which].GetNormal()+rayList[i].GetDirection();
            dir.Normalize();
            rayList[i].SetDirection(dir);
            rayList[i].SetOrigin(end);
            rayList[i].strength-=0.25f;
            glColor3f(1.0f,1.0f,0.0f);
        }
        else glColor3f(rayList[i].strength,0.0f,0.0f);
        dir = rayList[i].GetDirection();
        RayTracer::vector3 dist = end-listener;
        if(dist.Length()<=0.5f)
        {
            rayList[i].active=false;
            //std::cout<<"Hit# "<<rayList[i].microseconds<<"μs, Strength:"<<
            //    rayList[i].strength<<", Direction:"<<rayList[i].GetDirection()<<std::endl;
            glColor3f(1.0, 1.0, 0.0);
        }
        
        glVertex3f(rayList[i].GetOrigin().x, rayList[i].GetOrigin().y, rayList[i].GetOrigin().z);
        glVertex3f(end.x, end.y, end.z);
        glEnd();
        rayList[i].SetOrigin(rayList[i].GetOrigin()+rayList[i].GetDirection()*0.000340f);
        rayList[i].strength -= 0.00005f;
        if(rayList[i].strength<=0.0f) rayList[i].active=false;
        
    }
    if(active_cnt==0)
    {
        init();
        std::cout<<"New Wave"<<std::endl;
    }
    
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    scene.render();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);


    glutPostRedisplay();
    glFlush ();
	glutSwapBuffers();
}

void reshape (int w, int h)
{
    glViewport (0, 0, (GLsizei) w, (GLsizei) h);
    std::cout<<"width: "<<w<<" height:"<<h<<std::endl;
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glFrustum (-1.0*w/h, 1.0*w/h, -1.0, 1.0, 1.5, 20.0);
    glMatrixMode (GL_MODELVIEW);

	int tx, ty, tw, th;
	GLUI_Master.get_viewport_area( &tx, &ty, &tw, &th );
	glViewport( tx, ty, tw, th );

	xy_aspect = (float)tw / (float)th;

	glutPostRedisplay();
}

void mouse(int x,int y)
{
    rot_y=(x-300.f)/300.0f*180.0f;
    rot_x=-(y-200.f)/200.0f*180.0f/16.0f;
}
void recv_callback(int length, BYTE* recv)
{
    for(int i=0;i<length;++i)
    {

        BYTE b = recv[i];
        if (b<0) b+= 256;
        if (!start_flag)
        {
            if (b == start_mark[start_match_pos])
            {
                start_match_pos++;
                if (start_match_pos == 4)
                {
                    start_flag = true;
                    recv_cnt = 0;
                    start_match_pos = 0;
                }
            }
            else start_match_pos = 0;
        }
        else
        {

            recv_data[recv_cnt] = b;
            ++recv_cnt;
            if (recv_cnt == 26)
            {
                for (int i = 0; i < 26; i += 2)
                {
                    imu_result[i / 2] = (recv_data[i] << 8 | recv_data[i + 1]);
                    if (imu_result[i / 2] >= 32768)
                    {
                        imu_result[i / 2] -= 32768;
                        imu_result[i / 2] = -imu_result[i / 2];
                    }
                }
                recv_cnt = 0;

                /*ax = imu_result[0] / 10.0f;
                ay = imu_result[1] / 10.0f;
                az = imu_result[2] / 10.0f;

                gx = imu_result[3] / 10.0f;
                gy = imu_result[4] / 10.0f;
                gz = imu_result[5] / 10.0f;

                mx = imu_result[6] / 10.0f;
                my = imu_result[7] / 10.0f;
                mz = imu_result[8] / 10.0f;
                */
                yaw = imu_result[9] / 10.0f;
                pitch = imu_result[10] / 10.0f;
                roll = imu_result[11] / 10.0f;
                /*IMU.IMU_update(gx, gy, gz, ax, ay, az, mx, my, mz);
                if (cB_PC.Checked)
                {
                    yaw = IMU.yaw;
                    pitch = IMU.pitch;
                    roll = IMU.roll;
                }*/


                /*ry = -yaw;
                rz = pitch;
                rx = roll;
                */
                rot_z=-yaw;
                start_flag = false;
                recv_cnt = 0;
                start_match_pos = 0;
            }
        }
    }

}

void update()
{
	//init(); 
	initCalc();
	display();
	//playmusic();

}

int main(int argc, char** argv)
{
    std::cout<<"Select Serial Port(e.g COM1):";
    std::string com_port;
    std::cin>>com_port;
	//std::cout<<"Please input the music name\n";
	//std::cin>>music_file;
	//std::cout<<"Please input the scene name:\n";
	//std::cin>>scene_file;
	//std::cout<<"Chose a new song,please press the key 1、2、3、4\n";
    glutInit(&argc, argv);
    glutInitDisplayMode (GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize (800, 600);
    glutInitWindowPosition (50, 50);
    //glutCreateWindow (argv[0]);
	main_window=glutCreateWindow ("Ray Tracer for Sound Rendering 1.0");
    initCalc();
    init ();
    //HANDLE waveHandle = CreateThread(NULL,0,waveThread,(LPVOID)NULL,0,NULL);
    glutDisplayFunc(display);
	glutIdleFunc(display);
	
   // glutReshapeFunc(reshape);
	GLUI_Master.set_glutReshapeFunc( reshape );  
    glutPassiveMotionFunc(mouse);
    glutKeyboardFunc(keyinput);
	//glutKeyboardFunc( handleKeyDown );
	
    
    std::cout<<"Serial Port Status"<<serial.open(com_port);
    //std::cout<<serial.set_option(115200,0,8,0,0)<<"\n";
    //serial.recv_callback(recv_callback);
	
	//创建界面
	/*** Create the side subwindow ***/
	glui = GLUI_Master.create_glui_subwindow( main_window, 
					    GLUI_SUBWINDOW_RIGHT );
	glui->set_viewport();

	obj_panel = new GLUI_Panel(glui, "Scene" );
	GLUI_Panel *type_panel = new GLUI_Panel( obj_panel, "Type" );
	radio = new GLUI_RadioGroup(type_panel,&scene_type,3);
	new GLUI_RadioButton( radio, "Shoebox" );
	new GLUI_RadioButton( radio, "Two_box" );
	new GLUI_RadioButton( radio, "Gsound" );

	obj_panel = new GLUI_Panel(glui, "Audio" );
	GLUI_Panel *type_panel1 = new GLUI_Panel( obj_panel, "Type" );
	radio = new GLUI_RadioGroup(type_panel1,&audio_type,4);
	new GLUI_RadioButton( radio, "bird" );
	new GLUI_RadioButton( radio, "baby" );
	new GLUI_RadioButton( radio, "dry" );
	new GLUI_RadioButton( radio, "rap" );

	obj_panel = new GLUI_Panel(glui, "Source" );
	GLUI_Panel *type_panel2 = new GLUI_Panel( obj_panel, "Type" );
	radio = new GLUI_RadioGroup(type_panel2,&source_type,3);
	new GLUI_RadioButton( radio, "MC" );
	new GLUI_RadioButton( radio, "REA" );
	new GLUI_RadioButton( radio, "DEA" );

	obj_panel = new GLUI_Panel(glui, "Sound Propagation" );
	GLUI_Panel *type_panel3 = new GLUI_Panel( obj_panel, "Type" );
	radio = new GLUI_RadioGroup(type_panel3,&source_type,3);
	new GLUI_RadioButton( radio, "Specular" );
	new GLUI_RadioButton( radio, "Diffuse" );
	new GLUI_RadioButton( radio, "Both" );

	obj_panel = new GLUI_Panel(glui, "Computation" );
	GLUI_Panel *type_panel4 = new GLUI_Panel( obj_panel, "Type" );
	radio = new GLUI_RadioGroup(type_panel4,&source_type,5);
	new GLUI_RadioButton( radio, "RIR" );
	new GLUI_RadioButton( radio, "HRTF" );
	new GLUI_RadioButton( radio, "BRIR" );
	new GLUI_RadioButton( radio, "Energy Decay Curve" );
	new GLUI_RadioButton( radio, "IACC" );

	obj_panel = new GLUI_Panel(glui, "Audio Output" );
	GLUI_Panel *type_panel5 = new GLUI_Panel( obj_panel, "Form" );
	radio = new GLUI_RadioGroup(type_panel5,&source_type,3);
	new GLUI_RadioButton( radio, "WAV" );
	new GLUI_RadioButton( radio, "MP3" );
	new GLUI_RadioButton( radio, "MP4" );
	
	new GLUI_StaticText( glui, "" );
	GLUI_Button *play_control = new GLUI_Button( glui, "Audio Play", PLAY_ID, control_cb );
	play_control->set_w(4.0);
	//play_control->set_h(2.0);

	glui->set_main_gfx_window( main_window );

	/*** Create the bottom subwindow ***/
	glui2 = GLUI_Master.create_glui_subwindow( main_window, 
                                             GLUI_SUBWINDOW_BOTTOM );
	glui2->set_main_gfx_window( main_window );
	obj_panel = new GLUI_Rollout(glui2, "Option", false );
	GLUI_Spinner *spinner = 
	new GLUI_Spinner( obj_panel, "ray_num:", &ray_num);
	spinner->set_int_limits( 0, 10000 );
	spinner->set_alignment( GLUI_ALIGN_RIGHT );

	GLUI_Spinner *order_spinner = 
    new GLUI_Spinner( obj_panel, "order:", &order);
	order_spinner->set_int_limits( 0, 10 );
	order_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	GLUI_Spinner *radius_spinner = 
    new GLUI_Spinner( obj_panel, "radius:", &radius);
	radius_spinner->set_float_limits( 0.0f, 1.0f );
	radius_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	new GLUI_Column( glui2, false );
	obj_panel = new GLUI_Rollout(glui2, "Source Position", false );
	GLUI_Spinner *sx_spinner = 
    new GLUI_Spinner( obj_panel, "x:", &source_x);
	sx_spinner->set_float_limits( 0.0f, 100.0f );
	sx_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	GLUI_Spinner *sy_spinner = 
    new GLUI_Spinner( obj_panel, "y:", &source_y);
	sy_spinner->set_float_limits( 0.0f, 100.0f );
	sy_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	GLUI_Spinner *sz_spinner = 
    new GLUI_Spinner( obj_panel, "z:", &source_z);
	sz_spinner->set_float_limits( 0.0f, 100.0f );
	sz_spinner->set_alignment( GLUI_ALIGN_RIGHT );


	new GLUI_Column( glui2, false );
	obj_panel = new GLUI_Rollout(glui2, "Receiver Position", false );
	GLUI_Spinner *rx_spinner = 
    new GLUI_Spinner( obj_panel, "x:", &receiver_x);
	rx_spinner->set_float_limits( 0.0f, 100.0f );
	rx_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	GLUI_Spinner *ry_spinner = 
    new GLUI_Spinner( obj_panel, "y:", &receiver_y);
	ry_spinner->set_float_limits( 0.0f, 100.0f );
	ry_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	GLUI_Spinner *rz_spinner = 
    new GLUI_Spinner( obj_panel, "z:", &receiver_z);
	rz_spinner->set_float_limits( 0.0f, 100.0f );
	rz_spinner->set_alignment( GLUI_ALIGN_RIGHT );

	#if 0
	/**** We register the idle callback with GLUI, *not* with GLUT ****/
	GLUI_Master.set_glutIdleFunc(NULL );
	#endif
	
	//initializeScene();
    glutMainLoop();
    serial.close();
    return 0;
}
