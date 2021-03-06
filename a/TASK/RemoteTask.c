//注释了波轮和摩擦轮，无报错

#include "RemoteTask.h"
#include "stdio.h"
#include "SpeedTask.h"
#include "WorkState.h"
#include "ShootingTask.h"
#include "CanBusTask.h"
#include "InputState.h"

RC_Ctl_t RC_CtrlData;
RC_Ctl_t RC_lastCtrlData;
uint8_t D_Press = 0;
uint8_t A_Press = 0;
uint8_t Last_D_Press = 0;
uint8_t Last_A_Press = 0;
uint8_t F_Press = 0;
uint8_t Last_F_Press = 0;
uint8_t Fric_ON_flag= 0;
uint8_t Fric_OFF_flag= 0;

uint8_t laser_flag=0;
uint8_t press_flag=0;
uint8_t last_press_flag=0;
uint8_t Auto_Flag=0;
uint8_t Last_Auto_Flag=0;

double NOW_YAW_ANGLE ;
double NOW_PITCH_ANGLE ;

double mousexbefore;
double mousexafter=0;
double mouseyafter=0;
#define MOUSE_TO_PITCH_ANGLE_INC_FACT 		0.050f
#define MOUSE_TO_YAW_ANGLE_INC_FACT 		0.050f
#define MOUSE_TO_PITCH_SPEED_REF 		5.0f
#define MOUSE_TO_YAW_SPEED_REF 			5.0f

double MouseKeyX_EncoderFliter(double rawspeed);
double MouseKeyY_EncoderFliter(double rawspeed);
static const double fliter_num_10HZ[3] = {1.91119706742607f, -0.914975834801434f, 0.000944691843840151};
static const double fliter_num_25HZ[3] = {1.77863177782458f, -0.800802646665708f, 0.00554271721028068f};
static const double fliter_num_50HZ[3] = {1.56101807580072f, -0.641351538057563f, 0.0200833655642112f};
double pitch_err = 0;//2022加，p轴数据修正,传给视觉的
int pitch_err_flag = 0;//2022加 p轴数据修正标志位，只有标志位为1或-1且ch0为0时，pitch_err值加或减一个单位

/**
函数：RemoteDataPrcess(pData)
功能：对遥控器数据进行处理
备注：
        1                               1
   (s1) 3                          (s2) 3
	      2                               2
				
			(ch3)              (ch1)
       |                  |
       |                  |
	-----------(ch2)  -------------(ch0)     
			 |                  |
			 |                  |
		 遥控器测试模式时s2最上：
		 1.ch1控制底盘前后移动
		 2.ch2控制yaw水平旋转(累加、限位)
		 3.ch3控制pitch竖直摆动(累加、限位)
		 4.s1扳到最下面，全关,刹车回正
		 5.s2扳到中间，摩擦轮开，拨轮关
		 6.s2扳到最上，全开
		 
		 //遥控器测试模式s2=1：
		 1.ch1控制底盘前后移动
		 2.ch2控制yaw水平旋转(累加、限位)
		 3.ch3控制pitch竖直摆动(累加、限位)
		 4.s2=2,停止状态 s2=3,测试状态 s2=1,准备状态，2s后进入自由状态
		 5.s1控制刹车块
		 
**/
void RemoteDataPrcess(uint8_t *pData)
{

	if(pData == NULL)
	{
			return;
	}

//	GetRemoteSwitchAction(switch1, s1);
//	RemoteTest_Flag = 1;//遥控器打开时，遥控器测试标志变为1。
	Remote_microsecond.time_last = Get_Time_Micros();
	
	//ch0~ch3:max=1684,min=364,|error|=660
	RC_CtrlData.rc.ch0 = ((int16_t)pData[0] | ((int16_t)pData[1] << 8)) & 0x07FF; 
	RC_CtrlData.rc.ch1 = (((int16_t)pData[1] >> 3) | ((int16_t)pData[2] << 5)) & 0x07FF;
	RC_CtrlData.rc.ch2 = (((int16_t)pData[2] >> 6) | ((int16_t)pData[3] << 2) | ((int16_t)pData[4] << 10)) & 0x07FF;
	RC_CtrlData.rc.ch3 = (((int16_t)pData[4] >> 1) | ((int16_t)pData[5]<<7)) & 0x07FF;
	
	RC_CtrlData.rc.s1 = ((pData[5] >> 4) & 0x000C) >> 2;
	RC_CtrlData.rc.s2 = ((pData[5] >> 4) & 0x0003);
	
  RC_CtrlData.mouse.x = ((int16_t)pData[6]) | ((int16_t)pData[7] << 8);//鼠标左右
	RC_CtrlData.mouse.y = ((int16_t)pData[8]) | ((int16_t)pData[9] << 8);//鼠标上下
	RC_CtrlData.mouse.z = ((int16_t)pData[10]) | ((int16_t)pData[11] << 8);    //没用到

	RC_CtrlData.mouse.press_l = pData[12];//鼠标左键
	RC_CtrlData.mouse.press_r = pData[13];//鼠标右键

	RC_CtrlData.key.v = ((int16_t)pData[14]) | ((int16_t)pData[15] << 8);//每一位对应一个按键
	
	Chassis_Speed_Ref = (RC_CtrlData.rc.ch1-1024);
	GimbalRef.pitch_angle_dynamic_ref +=(RC_CtrlData.rc.ch3-1024)*0.003;//*0.01   /*原0.005*/
	GimbalRef.yaw_angle_dynamic_ref  +=(RC_CtrlData.rc.ch2-1024)*0.00285;//0.0085
	
	/****************修正数据函数************************/
	if((RC_CtrlData.rc.ch0-1024)>0){
		pitch_err_flag = 1;
	}
	else if((RC_CtrlData.rc.ch0-1024)<0){
		pitch_err_flag = -1;
	}
	if(pitch_err_flag==1&&(RC_CtrlData.rc.ch0-1024)<=0){
		pitch_err_flag=0;
		pitch_err+=0.5;
	}
	else if(pitch_err_flag==-1&&(RC_CtrlData.rc.ch0-1024)>=0){
		pitch_err_flag=0;
		pitch_err-=0.5;
	}
	
	/****************************************************/
	
//	if((RC_CtrlData.rc.s1==2)&&(GetFrictionState()==FRICTION_WHEEL_OFF))//打开摩擦轮

	
	//测发射延迟使用
//	if(RC_CtrlData.mouse.press_l==1 && RC_CtrlData.mouse.last_press_l ==0)
//	{
//		SetShootState(SHOOTING);
//		count_temp++;
//	}
//	RC_CtrlData.mouse.last_press_l = RC_CtrlData.mouse.press_l;
	SetInputMode(&RC_CtrlData.rc);
	
	if(RC_CtrlData.rc.s2==1)
	{
		RemoteTest_Flag = 2;//遥控器S2拨杆拨到1位置时，遥控器标志变为2，与工作状态对应
	}
	
	if(RC_CtrlData.rc.s2==3)//改为飞机后标志位1用来进入测试状态，故改为2
	{
		RemoteTest_Flag = 2;//遥控器打开时，遥控器测试标志变为2。拨杆拨到中间位置
	}
	
	if(RC_CtrlData.rc.s2==2)
	{
		RemoteTest_Flag = 0;//遥控器关闭时，遥控器测试状态变为0。拨杆拨到最下面
//		Brake_flag=0;//刹车回正
	}

		 //遥控器模式
	if(GetInputMode() == REMOTE_INPUT ){
	 
		if(RC_CtrlData.rc.s1==3|| RC_CtrlData.rc.s1==1)//打开摩擦轮，1.3
		{
			friction_wheel_state_flag = 1;
		}
	
	
		if(RC_CtrlData.rc.s1==2)  //关闭摩擦轮 2 刹车模块置于平衡
		{
			friction_wheel_state_flag = 0;
		  
		}  
	
		if(RC_CtrlData.rc.s1==1)//打开波轮 1
		{
			TempShootingFlag=1;
		}
	

		if(RC_CtrlData.rc.s1==2||RC_CtrlData.rc.s1==3)//关闭波轮 2.3
		{
		  TempShootingFlag=0;
		}
//	 switch(RC_CtrlData.rc.s1)//刹车模块位置测试（方向从板子方向向电池方向看）
//	 {
//		 case 2://(中间)
//		 { 
//			 Brake_flag=0;
//			 break;
//		 }
//		 case 3://（左面）
//		 {			
//			 Brake_flag=-1;
//			 break;
//		 }
//		 case 1://(最右面)
//		 {
//			 Brake_flag=1;
//			 break;
//		 }
//	 }
}	
 //键鼠模式
	else if( GetInputMode() == KEY_MOUSE_INPUT )
	{
		//Gun
		///////////////////////////////////发射///////////////////////////////////////////////////
//		if(RC_CtrlData.rc.s1==2)  //关闭摩擦轮 2
//		{
//				friction_wheel_state_flag = 0;
//			  TempShootingFlag=0;
//		}
//		else 	
		{
		//Friction			
			friction_wheel_state_flag = 1;			
		 //SHOOTING
			TempShootingFlag=1;
	  }
		 ///////////////////////////////////发射///////////////////////////////////////////////////			
		//Gimbal
		///////////////////////////////////云台///////////////////////////////////////////////////
	{
		if((RC_CtrlData.key.v & 0x0001)||(RC_CtrlData.key.v & 0x0002))
		{
			if(RC_CtrlData.key.v & 0x0001)//定住YAW轴 W
			{
			GimbalRef.yaw_angle_dynamic_ref  += 0;
			GimbalRef.pitch_angle_dynamic_ref += RC_CtrlData.mouse.y* MOUSE_TO_PITCH_ANGLE_INC_FACT;	
			}
			
			if(RC_CtrlData.key.v & 0x0002)//定住PITCH轴 S
			{
				GimbalRef.yaw_angle_dynamic_ref  += RC_CtrlData.mouse.x* MOUSE_TO_YAW_ANGLE_INC_FACT;			
				GimbalRef.pitch_angle_dynamic_ref += 0;
			}
		}
		else
		{
			GimbalRef.yaw_angle_dynamic_ref  += RC_CtrlData.mouse.x* MOUSE_TO_YAW_ANGLE_INC_FACT;				
			GimbalRef.pitch_angle_dynamic_ref += RC_CtrlData.mouse.y* MOUSE_TO_PITCH_ANGLE_INC_FACT;			
		}
		
//		else
//		{			
//			mousexbefore=RC_CtrlData.mouse.x;
//			mousexafter=MouseKeyX_EncoderFliter(RC_CtrlData.mouse.x );
//			GimbalRef.yaw_angle_dynamic_ref  += mousexafter* MOUSE_TO_YAW_ANGLE_INC_FACT;
//			
//			mouseyafter=MouseKeyY_EncoderFliter(RC_CtrlData.mouse.y);
//			GimbalRef.pitch_angle_dynamic_ref += mouseyafter* MOUSE_TO_PITCH_ANGLE_INC_FACT;		
//			
//			NOW_YAW_ANGLE = GMYawEncoder.ecd_angle;
//			NOW_PITCH_ANGLE = GMPitchEncoder.ecd_angle;
//		}
		
		//自瞄
		
		if(RC_CtrlData.mouse.press_r)
		{
			Auto_Flag = 1;
		}
		else
		{
			Auto_Flag = 0;
		}		
		Last_Auto_Flag = Auto_Flag;
	}
		///////////////////////////////////云台///////////////////////////////////////////////////
		//激光
		///////////////////////////////////激光///////////////////////////////////////////////////
		{
			if(RC_CtrlData.key.v & 0x0100)//key: r 激光
			{	
				press_flag=1;
			}
			else 
			{
				press_flag=0;
			}
			if(press_flag==0 && last_press_flag==1)
			{
				if(laser_flag==0)
				{
					laser_flag = 1;
				}
				else if(laser_flag==1)
				{
					laser_flag = 0;
				}
			}
		
			if(laser_flag == 1)
			{
				laser_on();
			}
			else if(laser_flag == 0)
			{
				laser_off();
			}
	
			last_press_flag = press_flag;
		}
		///////////////////////////////////激光///////////////////////////////////////////////////
	}	
	//RC_lastCtrlData.mouse.press_r = RC_CtrlData.mouse.press_r;
}

double MouseKeyX_EncoderFliter(double rawspeed)
{
		static double Input3510[3] = {0.0f,0.0f,0.0f};
		static double Output3510[3] = {0.0f,0.0f,0.0f};
		
		Input3510[0] = Input3510[1]; 
		Input3510[1] = Input3510[2]; 
		Input3510[2] = rawspeed * fliter_num_50HZ[2];
		Output3510[0] = Output3510[1]; 
		Output3510[1] = Output3510[2]; 
		Output3510[2] = (Input3510[0] + Input3510[2]) + 2 * Input3510[1] + (fliter_num_50HZ[1] * Output3510[0]) + (fliter_num_50HZ[0] * Output3510[1]);
		return Output3510[2];
}

double MouseKeyY_EncoderFliter(double rawspeed)
{
		static double Input3510[3] = {0.0f,0.0f,0.0f};
		static double Output3510[3] = {0.0f,0.0f,0.0f};
		
		Input3510[0] = Input3510[1]; 
		Input3510[1] = Input3510[2]; 
		Input3510[2] = rawspeed * fliter_num_50HZ[2];
		Output3510[0] = Output3510[1]; 
		Output3510[1] = Output3510[2]; 
		Output3510[2] = (Input3510[0] + Input3510[2]) + 2 * Input3510[1] + (fliter_num_50HZ[1] * Output3510[0]) + (fliter_num_50HZ[0] * Output3510[1]);
		return Output3510[2];
}


 

 
 
