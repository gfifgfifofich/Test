#include <iostream>
#include <vector>
// for simulation
#include <thread>
#include <chrono>

// i dont cnow what cpus are used and how they work, but they are probabply not better then game console, 
// so no pointers were used, only arrays, messages are compressed to 4 bytes
// its overkill for sure, but this means that it will work with more stability 

// fast connection means that every trafic light will be able to have full data about every other light consistantly. 
// each light can throw messages with delay from others to the network with udp-like protocol to every other
// ->12 mesages total will be sent per 1 "step"
// each light needs to send only 1 message per step
// currently each light throws message 11 times per step (44 bytes), to every light individually (because of task), but it still should be fine 

/* id structure of trafic lights    
	   4   5
	11   0	 6
	   3   1
	10   2   7
	   8   9
*/


// a message struct. size 4 bytes. i dont knowwhat connection type is, but smaller Message = faster
struct Message
{
	// sender/target == -1 -> invalid package
	int8_t senderid = -1;
	int8_t targertid = -1;
	// data == -1 -> no data was sent
	int8_t senderState = -1;
	int8_t senderqueueSize = -1;
};

// for easier movement through ID  "clockwise" or "counter clockwise"
int IdLoopPed(int8_t id, int8_t value)
{
	id += value;
	//while loops act like if statements most of the time
	while (id>11)
		id -=8;
	while (id<4)
		id +=8;
	return id;
}
int IdLoopCars(int8_t id, int8_t value)
{
	id += value;
	//while loops act like if statements most of the time
	while (id>4)
		id -=4;
	while (id<0)
		id +=4;
	return id;
}

// all lights are same class, pedestrian are id>3 || bool pedestrian = true
class TLight
{
public:
	enum state
	{
		RED=0,
		YELLOW=1,
		GREEN=2
	};

	int8_t id = -1;
	int state = GREEN;// full int because visualizer
	int queueSize = 0;// full int because visualizer
	int8_t states[11];
	int8_t queueSizes[11];// 127 queue size should be enough
	
	float timer = 0.0f; // <= 0.0f == no tasks + empty queue
	float wait = 3.0f; // in case of heavy traffic, force green light

	//simulation of network
	std::vector<Message> Messages;
	void SendMessage(unsigned int targetid);

	void ProcessMessages()
	{

		for(auto m : Messages)
		{
			if(m.targertid == id && m.senderid>-1)
			{
				states[m.senderid] = m.senderState;
				queueSizes[m.senderid] = m.senderqueueSize;
			}
		}
		Messages.clear();
	}
	// process, dt - time passed (Delta time)
	// for demonstration, every timer is like 3 secs
	// all red
	// entity appears
	// block all affected lanes, green this lane
	void Process(float dt)
	{	
		ProcessMessages();
		// each car can go forward or right (clockwise), each lane is straight
		


		// task workaround
		for(int i=0;i<12;i++)
			if(i !=id)
				SendMessage(i);

		timer -=dt;
		if(timer > 0.0f &&timer < 1.0f && id<4)// only if carlight
			state = YELLOW;
		if(timer > 0.0f && state==GREEN)
		{
			wait = 6.0f;//reset wait timer
			return;
		}
		if(timer <0.0f)
			timer = 0.0f;
		// could've made stuff with inheritance and more classes to avoid this one if statement and add 1000 virtual tables (virtual functions) -> slower
		if(id>3)
		{//pedastrian light
			/* id structure of trafic lights    
			   4   5
			11    	 6
			        
			10       7
			   8   9
			*/
			int mirrorid = id+1;
			if(id%2 != 0)
				mirrorid -=2;
				
			int lane = (id/2) - 2;// this will give id of lane, same layout as car lights
			int affectedCarlaneIds[3]; // in case lane == 0, [0,1,2], etc.
			for(int i=0;i<3;i++)
				affectedCarlaneIds[i] = IdLoopCars(lane,i);

			bool canGo = true;
			for(int i=0;i<3;i++)
				if((queueSizes[affectedCarlaneIds[i]] > 0 && states[affectedCarlaneIds[i]] == GREEN))
					canGo = false;

			if (canGo)
			{
				timer = 3.0f;
				state = GREEN;
			}
			else // if unable, force timer
			{
				state = RED;
				wait -= dt;
			}

		}
		else
		{
			/*
			  0
			3   1
			  2
			*/
			// get perpendicular lanes
			int affectedCarlaneIds[2]; 
			if(id%2 == 0)
			{
				affectedCarlaneIds[0] = 1;
				affectedCarlaneIds[1] = 3;
			}
			else
			{
				affectedCarlaneIds[0] = 0;
				affectedCarlaneIds[1] = 2;
			}
			int lane = (id*2) + 4;// this will give id of pedastrian lane
			int affectedPedlaneIds[6]; 
			for(int i=0;i<6;i++)
				affectedPedlaneIds[i] = IdLoopPed(id,lane-i+1);
			bool canGo = true;
			for(int i=0;i<2;i++)
				if((queueSizes[affectedCarlaneIds[i]] > 0 && states[affectedCarlaneIds[i]] == GREEN))
					canGo = false;
			for(int i=0;i<6;i++)
				if((queueSizes[affectedPedlaneIds[i]] > 0 && states[affectedPedlaneIds[i]] == GREEN))
					canGo = false;
			if (canGo)
			{
				timer = 6.0f;
				state = GREEN;
			}
			else // if unable, force timer
			{
				wait -= dt;
				if(wait<=0.0f)
				{ 
					bool deadlockCheck = false;
					for(int i=0;i<3;i++)
						if(states[affectedCarlaneIds[i]] == YELLOW)
							deadlockCheck = true;
					if(deadlockCheck)
						state = YELLOW;// this should force logic for others to wait this light
				}	
				else
					state = RED;
			}
			
		}

	}
};
TLight lights[12];

//simulation of network
void TLight::SendMessage(unsigned int targetid)
{
	Message m;
	m.senderid = id;
	m.targertid = targetid;
	m.senderState = state;
	m.senderqueueSize = queueSize;
	for(int i=0;i<12;i++)
	{
		if(lights[i].id == targetid)
			lights[i].Messages.push_back(m);
	}
}

int main()
{
	TLight l;
	//4 lights for cars, clockwise
	for(int i =0;i<12;i++)
	{
		l.id = i;
		lights[i] = l;
	}

	// simulation
	while (true)
	{
		//chance of new traffic
		for(int i=0;i<5;i++)
			if(rand()%100 >= 50)
			{
				// add random amount to random trafic light
				int id = rand() % 12;
				int amount = rand() % 3;
				lights[id].queueSize += amount;
			}
		// simulate 1 second further
		for(int i =0;i<12;i++)
			lights[i].Process(1.0f);


		for(int i =0;i<12;i++)
		{
			if(lights[i].state == TLight::GREEN && lights[i].queueSize >0)
				lights[i].queueSize -=1;
		}
		/* id structure of trafic lights    
			   4   5
			11   0	 6
			   3   1
			10   2   7
			   8   9
		*/
		// cursed visualiser
		std::cout<< "\n";
		std::cout<<"    "<<lights[4].queueSize << " "<<lights[4].state <<"           "<<lights[5].queueSize << " "<<lights[5].state << "\n";
		std::cout<< "\n";
		std::cout<<lights[11].queueSize <<" "<<lights[11].state <<"        "<<lights[0].queueSize << " "<<lights[0].state <<"        "<<lights[6].queueSize << " "<<lights[6].state << "\n";
		std::cout<< "\n";
		std::cout<<"    "<<lights[3].queueSize << " "<<lights[3].state <<"           "<<lights[1].queueSize << " "<<lights[1].state << "\n";
		std::cout<< "\n";
		std::cout<<lights[10].queueSize << " "<<lights[10].state <<"        "<<lights[2].queueSize << " "<<lights[2].state <<"        "<<lights[7].queueSize << " "<<lights[7].state << "\n";
		std::cout<< "\n";
		std::cout<<"    "<<lights[8].queueSize << " "<<lights[8].state <<"           "<<lights[9].queueSize << " "<<lights[9].state << "\n";
		std::cout<< "\n";
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1000ms);
	}


}