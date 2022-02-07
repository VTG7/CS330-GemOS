#include "wc.h"


extern struct team teams[NUM_TEAMS];
extern int test;
extern int finalTeam1;
extern int finalTeam2;

int processType = HOST;
const char *team_names[] = {
  "India", "Australia", "New Zealand", "Sri Lanka",   // Group A
  "Pakistan", "South Africa", "England", "Bangladesh" // Group B
};


void teamPlay(void)
{
  chdir("test");
  char test_no[16] = {'\0'};
  sprintf(test_no,"%d",test);
  strcat(test_no,"/inp");
  chdir(test_no); 
  int fd = open(teams[processType].name, O_RDONLY);
  if(fd<0) printf("No INput\n ");
  char buf1[15] = "dontterminate";
  write(teams[processType].commpipe[1], buf1, 15);
  while(1) {
    char buf[1];
    read(fd,buf,1);
    
    write(teams[processType].matchpipe[1], buf, 1);
    //printf*("matchpipe Buffer has %s\n", buf);
    char buf2[15]={'\0'};
    read(teams[processType].commpipe[0], buf2, 15);// this is waiting

    if(!strcmp(buf2,"terminate")) {
     // printf("Works\n");
      exit(0);
    }
    else {
      write(teams[processType].commpipe[1], buf1, 15);
    }

  }

}

void endTeam(int teamID)
{
  char terminate[15] = "terminate";
  write(teams[teamID].commpipe[1], terminate, 15);
  
}

int match(int team1, int team2)
{
  chdir("test");
  char test_no[16] = {'\0'};
  sprintf(test_no,"%d",test);
  strcat(test_no,"/out");
  chdir(test_no);
  //printf*("Match happening between %d and %d\n",team1,team2);
  char buf[12] = {'\0'};
  read(teams[team1].matchpipe[0], buf, 1);
  int n1 = atoi(buf);
  read(teams[team2].matchpipe[0], buf, 1);
  
  int n2 = atoi(buf);
  int sum = n1+n2;
  int bat, ball;
  if(sum%2) {
    // team 1 bats
    bat = team1;
    ball = team2;
  }
  else {
    // team 2 bats
    bat = team2;
    ball = team1;
  }
  char batteam[50] ={'\0'}; 
  strcpy(batteam, teams[bat].name);
  char ballteam[50] = {'\0'};
  strcpy(ballteam,teams[ball].name);
  strcat(batteam, "v");
  strcat(batteam, ballteam);
  if(team1<4 && team2>=4) {
    // diff groups => final match
    strcat(batteam,"-Final");
  }
  int fd = open(batteam, O_RDWR | O_CREAT, 0777);
  if(fd<1) printf("op error\n");
  char append[100] = "Innings1: ";
  strcat(append, teams[bat].name);
  strcat(append, " bats");
  
  write(fd, append, strlen(append));

  int wickets1 = 0;
  int cur_bat_run = 0;
  int runs1 = 0;
  for(int i = 0;i<120;i++) {
    char buf[12] = {'\0'};
    
    read(teams[bat].matchpipe[0], buf, 1);
    //printf*("checker log\n");
    int dbat = atoi(buf);
    read(teams[ball].matchpipe[0], buf, 1);
    int dball = atoi(buf);
    if(dbat!=dball) {
      runs1+=dbat;
      cur_bat_run+=dbat;
    }
    else {
      wickets1++;
      write(fd,"\n",1);
      char batsman[12] = {'\0'};
      sprintf(batsman, "%d", wickets1);

      strcat(batsman, ":");
      char run[12] = {'\0'};
      sprintf(run, "%d", cur_bat_run);
      strcat(batsman, run);
      write(fd, batsman,strlen(batsman) );
      cur_bat_run = 0;
    }
    if(wickets1==10) break;

  }
  if(wickets1!=10) {
      wickets1++;
      write(fd,"\n",1);
      char batsman[12] = {'\0'};
      sprintf(batsman, "%d", wickets1);
      strcat(batsman, ":");
      char run[12] = {'\0'};
      sprintf(run, "%d", cur_bat_run);      
      strcat(batsman, run);
      strcat(batsman, "*");
      write(fd, batsman,strlen(batsman) );
      cur_bat_run = 0;   
  }
  write(fd, "\n",1);
  char addendum[50] = {'\0'};
  strcpy(addendum,teams[bat].name);
  strcat(addendum, " TOTAL: ");
  char temp1[12] = {'\0'};
  sprintf(temp1, "%d", runs1);
  strcat(addendum,temp1 );
  write(fd, addendum, strlen(addendum));

  write(fd, "\n",1);
  char append2[100] = "Innings2: ";
  strcat(append2, teams[ball].name);
  strcat(append2, " bats");
  write(fd, append2, strlen(append2));

  // team 2 bats now:

  wickets1 = 0;
  cur_bat_run = 0;
  int runs2 = 0;
  for(int i = 0;i<120;i++) {
    char buf[12] = {'\0'};
    read(teams[ball].matchpipe[0], buf, 1);
    int dbat = atoi(buf);
    read(teams[bat].matchpipe[0], buf, 1);
    int dball = atoi(buf);
    if(dbat!=dball) {
      runs2+=dbat;
      cur_bat_run+=dbat;
    }
    else {
      wickets1++;
      write(fd,"\n",1);
      char batsman[12] = {'\0'};
      sprintf(batsman, "%d", wickets1);      
      strcat(batsman, ":");
      char run[12] = {'\0'};
      sprintf(run, "%d", cur_bat_run); 
      strcat(batsman, run);
      write(fd, batsman,strlen(batsman) );
      cur_bat_run = 0;
    }
    if(runs2>runs1) {
      // bowling team wins
      if(wickets1!=10) {
      wickets1++;
      write(fd,"\n",1);
      char batsman[12] = {'\0'};
      sprintf(batsman, "%d", wickets1);
      strcat(batsman, ":");
      char run[12] = {'\0'};
      sprintf(run, "%d", cur_bat_run);      
      strcat(batsman, run);
      strcat(batsman, "*");
      write(fd, batsman,strlen(batsman) );
      cur_bat_run = 0;   
    } 
        write(fd, "\n",1);
        char addendum2[50] = {'\0'};
        strcpy(addendum2,teams[ball].name);
        strcat(addendum2, " TOTAL: ");
        char temp12[12] = {'\0'};
        sprintf(temp12, "%d", runs2);
        strcat(addendum2,temp12 );
        write(fd, addendum2, strlen(addendum2));

      write(fd, "\n",1);
      char  win[100] = {'\0'};
      strcpy(win,teams[ball].name);
      strcat(win, " beats ");
      strcat(win, teams[bat].name);
      strcat(win, " by ");
      char temp2[12] = {'\0'};
      sprintf(temp2, "%d", 11-wickets1);
      strcat(win, temp2);
      strcat(win, " wickets");
      write(fd, win, strlen(win));
      write(fd, "\n",1);
      close(fd);
      return ball;


    }
    if(wickets1==10) break;

  }
  if(wickets1!=10) {
      wickets1++;
      write(fd,"\n",1);
      char batsman[12] = {'\0'};
      sprintf(batsman, "%d", wickets1); 
      strcat(batsman, ":");
      char run[12] = {'\0'};
      sprintf(run, "%d", cur_bat_run); 
      strcat(batsman, run);
      strcat(batsman, "*");
      write(fd, batsman,strlen(batsman) );
      cur_bat_run = 0;   
  }
  if(runs2 <runs1) { // batting team wins
  write(fd, "\n",1);
        char addendum2[50] = {'\0'};
        strcpy(addendum2,teams[ball].name);
        strcat(addendum2, " TOTAL: ");
        char temp12[12] = {'\0'};
        sprintf(temp12, "%d", runs2);
        strcat(addendum2,temp12 );
        write(fd, addendum2, strlen(addendum2));
      write(fd, "\n",1);
      char  win[100] = {'\0'};
      strcpy(win,teams[bat].name);
      strcat(win, " beats ");
      strcat(win, teams[ball].name);
      strcat(win, " by ");
      char temp3[12]={'\0'};
      sprintf(temp3, "%d", runs1-runs2);
      strcat(win, temp3);
      strcat(win, " runs");
      write(fd, win, strlen(win));
      write(fd, "\n",1);
      close(fd);
      return bat;
  }
  else if(runs2==runs1) { //tie case
      //team 1 wins always
      write(fd, "\n",1);
        char addendum2[50] = {'\0'};
        strcpy(addendum2,teams[ball].name);
        strcat(addendum2, " TOTAL: ");
        char temp12[12] = {'\0'};
        sprintf(temp12, "%d", runs2);
        strcat(addendum2,temp12 );
        write(fd, addendum2, strlen(addendum2));
      write(fd, "\n",1);
      char  win[100] = "TIE: ";
      strcat(win, teams[team1].name);
      strcat(win, " beats ");
      strcat(win, teams[team2].name);
      write(fd, win, strlen(win));
      write(fd, "\n",1);
      close(fd);
      return team1;   
  }
	return 0;
}

void spawnTeams(void)
{
  
  int pid;
  int i;
	for(i = 0;i<NUM_TEAMS;i++) {
    strcpy(teams[i].name, team_names[i]);
    pipe(teams[i].matchpipe);   
    pipe(teams[i].commpipe);
    pid = fork();
    // handling the pipes 
    if(pid>0) {
     // close(teams[i].commpipe[0]);  // closing the commpipe read end in the parent;
     // close(teams[i].matchpipe[1]);  // closing the matchpipe write end in the parent;

    }
    //wait(NULL);
    if(!pid) break;

  }
  if(pid==0) {
    processType = i;

    //close(teams[i].commpipe[1]); // closing the commpipe write end in the child;
    //close(teams[i].matchpipe[0]);  // closing the matchpipe read end in the child;
    //printf*("%d\n", processType);
    teamPlay(); 
  } 
  //wait(NULL);
  else if(pid!=0) {
    //for(int i = 0;i<8;i++) endTeam(i);
   return;
  }
}

void conductGroupMatches(void)
{
  int pid;
  int group1pipe[2],group2pipe[2];
  int i;
  for( i = 0;i<2;i++){  // group processes
    if(i==0) {
      pipe(group1pipe);
      pid = fork();

      if(!pid) break;
    }
    else if(i==1) {
      pipe(group2pipe);
      pid = fork();
      if(!pid) break;
    }
  }
  if(pid == 0) {        // group processes
    if(i==0){  // group 1
      // handle pipes:
      //printf*("Group 1 initiated \n");
      // conduct matches:
      int scoreg1[4]={0};
      for(int i = 0;i<4;i++) {
        for(int j=i+1;j<4;j++) {
          //sleep(3);

          scoreg1[match(i,j)]++;// match conducted between i and j
        }
      }
      
    
      
      int leader = 0;
      for(int i = 1;i<4;i++) {
        if(scoreg1[i]>scoreg1[leader]) leader = i;
      }
      char buf[12] = {'\0'};
      sprintf(buf,"%d",leader);
      write(group1pipe[1],buf,1);
      for(int i = 0;i<4;i++) {
        if(i!=leader) endTeam(i);
      }


     exit(0);
    }
    else if(i==1){  // group 2
      // conduct matches:
      //printf*("Group 2 initiated\n");

      int scoreg2[4]={0};
      for(int i = 4;i<8;i++) {
        for(int j=i+1;j<8;j++) {
          //sleep(3);
          scoreg2[match(i,j)-4]++;// match conducted between i and j
        }
      }
      
      int leader = 0;
      for(int i = 1;i<4;i++) {
        if(scoreg2[i]>scoreg2[leader]) leader = i;
      }
      leader+=4;
      char buf[12] = {'\0'};
      sprintf(buf,"%d",leader);
      write(group2pipe[1],buf,1);  
      for(int i = 4;i<8;i++) {
        if(i!=leader) endTeam(i);
      }
      //printf("Group 2 leader is %d\n", leader);
      exit(0);
    }
 
 
  }
  char buf[1] = {'\0'};
  read(group1pipe[0], buf, 1);
  finalTeam1 = atoi(buf);
  read(group2pipe[0], buf, 1);
  finalTeam2 = atoi(buf);


}

