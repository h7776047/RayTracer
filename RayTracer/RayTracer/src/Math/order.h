#ifndef ORDER_H
#define ORDER_H

#include <iostream>
#include <vector>

namespace RayTracer{
 
	void order(std::vector<float>& data)
	{
	 int count=data.size();
	 int tag=false;
	 for(int i=0;i<count;i++)
	 {
	  for(int j=0;j<count-i-1;j++)
	  {
	   if(data[j]>data[j+1])
	   {
	    tag=true;
		int temp=data[j];
		data[j]=data[j+1];
		data[j+1]=temp;
	   }
	  }
	  if(!tag)
		  break;
	 }
	}
}





#endif