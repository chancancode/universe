/****
   universe.c
   version 2
   Richard Vaughan  
****/

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "universe.h"
using namespace Uni;

const char* PROGNAME = "universe";

#if GRAPHICS
#include <GLUT/glut.h> // OS X users need <glut/glut.h> instead
#endif

// initialize static members
double Robot::worldsize(1.0);
double Robot::range( 0.1 );
double Robot::fov(  dtor(270.0) );
std::vector<Robot*> Robot::population;
unsigned int Robot::population_size( 100 );
unsigned int *Robot::sectors_pop;
unsigned int *Robot::sectors;
int Robot::num_of_sectors( 1 );
double Robot::sector_width( 0.1 );
unsigned int Robot::pixel_count( 8 );
unsigned int Robot::sleep_msec( 50 );
uint64_t Robot::updates( 1 );
uint64_t Robot::updates_max( 0.0 ); 
bool Robot::paused( false );
int Robot::winsize( 600 );
int Robot::displaylist( 0 );
bool Robot::show_data( true );
int Robot::num_processes( 1 );

// Shared stuff
double *Robot::pose;
double *Robot::pose_next;
int *Robot::grid;
int *Robot::grid_next;

// Semaphores
sem_t **sem_done;
sem_t **sem_ready;

// Unique squential process ID
int spid;

char usage[] = "Universe understands these command line arguments:\n"
 "  -? : Prints this helpful message.\n"
 "  -c <int> : sets the number of pixels in the robots' sensor.\n"
 "  -d  Disables drawing the sensor field of view. Speeds things up a bit.\n"
 "  -f <float> : sets the sensor field of view angle in degrees.\n"
 "  -p <int> : set the size of the robot population.\n"
 "  -r <float> : sets the sensor field of view range.\n"
 "  -s <float> : sets the side length of the (square) world.\n"
 "  -u <int> : sets the number of updates to run before quitting.\n"
 "  -w <int> : sets the initial size of the window, in pixels.\n"
 "  -z <int> : sets the number of milliseconds to sleep between updates.\n"
 "  -n <int> : sets the number of processes to spawn.\n";

#if GRAPHICS
// GLUT callback functions ---------------------------------------------------

static void timer_func( int dummy )
{
  glutPostRedisplay(); // force redraw
}

// draw the world - this is called whenever the window needs redrawn
static void display_func( void ) 
{  
  glClear( GL_COLOR_BUFFER_BIT );
  
  // Draw the checker board
  for(int i=0;i<Robot::num_of_sectors;i++)
      for(int j=0;j<Robot::num_of_sectors;j++)
      {
          glBegin( GL_POLYGON );
          ((i%2+j%2)%2 == 0)? glColor3f( 0.9,0.9,1 ) : glColor3f( 1,0.9,0.9 );
          glVertex2f( i*Robot::sector_width,j*Robot::sector_width );
          glVertex2f( (i+1)*Robot::sector_width,j*Robot::sector_width );
          glVertex2f( (i+1)*Robot::sector_width,(j+1)*Robot::sector_width );
          glVertex2f( i*Robot::sector_width,(j+1)*Robot::sector_width );
          glEnd();
      }
  
  Robot::DrawAll();
  glutSwapBuffers();
	
  // run this function again in about 50 msec
  glutTimerFunc( 20, timer_func, 0 );
}

static void mouse_func(int button, int state, int x, int y) 
{  
  if( (button == GLUT_LEFT_BUTTON) && (state == GLUT_DOWN ) )
	 {
        pthread_mutex_lock(&mutex);
		if(Robot::paused && working_threads == 0){
		    //fprintf( stderr, "Main thread is waking everyone up...\n" );
            working_threads = Robot::num_threads;
		    pthread_cond_broadcast(&cond);
		}
		Robot::paused = !Robot::paused;
        pthread_mutex_unlock(&mutex);
	 }
}

// render all robots in OpenGL
void Robot::DrawAll()
{
	FOR_EACH( r, population )
		(*r)->Draw();
}

#endif // GRAPHICS

Robot::Robot(double x, double y, double a, double r, double g, double b)
  : pixels(), speed()
{
  // add myself to the static vector of all robots
  id = population.size();
  population.push_back( this );

  pose_next[posei(id,0)] = x;
  pose_next[posei(id,1)] = y;
  pose_next[posei(id,2)] = a;

  grid_next[gridi(id,0)] = floor(pose_next[posei(id,0)]/sector_width);
  grid_next[gridi(id,1)] = floor(pose_next[posei(id,1)]/sector_width);

  pixels.resize( pixel_count );
}

void *shm_mmap(size_t size)
{
    int shm_fd;
    
    // open the SHM
    shm_fd = shm_open("kfc5.universe3.shm", O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
    
    if (shm_fd == -1){
        fprintf( stderr, "Cannot open SHM... Error: %d, %s\n", errno, strerror(errno) );
        close(shm_fd);
        sem_unlink("kfc5.universe3.shm");
        exit(-1);
    }
    
    // truncate
    if (ftruncate(shm_fd, size) == -1){
        fprintf( stderr, "Cannot truncate SHM... Error: %s\n", strerror(errno) );
        close(shm_fd);
        shm_unlink("kfc5.universe3.shm");
        exit(-1);
    }
    
    // mmap
    void *ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (ptr == MAP_FAILED){
        fprintf( stderr, "Cannot map shared memory... Error: %s\n", strerror(errno) );
        close(shm_fd);
        shm_unlink("kfc5.universe3.shm");
        exit(-1);
    } else {
        close(shm_fd);
        shm_unlink("kfc5.universe3.shm");
        return ptr;
    }
}

sem_t *sem_get(const char* prefix, int i)
{
    char name[50];
    
    sprintf(name, "kfc5.universe3.sem.%s%d", prefix, i);
    
    sem_t *ptr = sem_open(name, O_EXCL|O_CREAT, 0600, 0);

    if(ptr == SEM_FAILED){
        fprintf( stderr, "Cannot open semaphore %s... Error: %s\n", name, strerror(errno) );
        sem_unlink(name);
        exit(-1);
    }

    sem_unlink(name);
    
    return ptr;
}

void Robot::Init( int argc, char** argv )
{
  // seed the random number generator with the current time
  srand48(0);
	
  // parse arguments to configure Robot static members
	int c;
	while( ( c = getopt( argc, argv, "?dp:s:f:r:c:u:z:n:w:")) != -1 )
		switch( c )
			{
			case 'p': 
				population_size = atoi( optarg );
				fprintf( stderr, "[Uni] population_size: %d\n", population_size );
				break;
				
			case 's': 
				worldsize = atof( optarg );
				fprintf( stderr, "[Uni] worldsize: %.2f\n", worldsize );
				break;
				
			case 'f': 
				fov = dtor(atof( optarg )); // degrees to radians
				fprintf( stderr, "[Uni] fov: %.2f\n", fov );
				break;
				
			case 'r': 
				range = atof( optarg );
				fprintf( stderr, "[Uni] range: %.2f\n", range );
				break;
				
			case 'c':
				pixel_count = atoi( optarg );
				fprintf( stderr, "[Uni] pixel_count: %d\n", pixel_count );
				break;
				
            case 'u':
				updates_max = atol( optarg );
				fprintf( stderr, "[Uni] updates_max: %lu\n", (long unsigned)updates_max );
				break;
				
			case 'z':
				sleep_msec = atoi( optarg );
				fprintf( stderr, "[Uni] sleep_msec: %d\n", sleep_msec );
				break;
				
			case 'n':
                num_processes = atoi( optarg );
                fprintf( stderr, "[Uni] num_processes: %d\n", num_processes );
                break;
                
#if GRAPHICS
			case 'w': winsize = atoi( optarg );
				fprintf( stderr, "[Uni] winsize: %d\n", winsize );
				break;

			case 'd': show_data= false;
			  fprintf( stderr, "[Uni] hide data" );
			  break;
#endif			
			case '?':
			  fprintf( stderr, usage );
			  exit(0); // ok
			  break;

			default:
				fprintf( stderr, "[Uni] Option parse error.\n" );
				fprintf( stderr, usage );
				exit(-1); // error
		}
		
	// initialize the population and sectors
	num_of_sectors = floor(worldsize / range);
    sector_width = worldsize / num_of_sectors;
    population.reserve(population_size);
    
    sectors_pop = (unsigned int *) shm_mmap(sizeof(unsigned int)*num_of_sectors*num_of_sectors);
    sectors = (unsigned int *) shm_mmap(sizeof(unsigned int)*num_of_sectors*num_of_sectors*population_size);
    
    //fprintf( stderr, "Mapped sectors: %p, %p\n", sectors_pop, sectors);
    
    for(int i=0;i<num_of_sectors;i++)
        for(int j=0;j<num_of_sectors;j++)
            sectors_pop[secpi(i,j)] = 0;
    
    //fprintf( stderr, "Cleared sectors\n" );
    
	// initialize the buffers
    pose = (double *) shm_mmap(sizeof(double)*population_size*3);
    pose_next = (double *) shm_mmap(sizeof(double)*population_size*3);
    
    grid = (int *) shm_mmap(sizeof(int)*population_size*2);
    grid_next = (int *) shm_mmap(sizeof(int)*population_size*2);
    
    //fprintf( stderr, "Mapped buffers: %p, %p, %p, %p\n", pose, pose_next, grid, grid_next );
    
    // initialize the sequential pid
    spid = num_processes;

    // initialize the semaphores
    sem_done = new sem_t*[num_processes];
    sem_ready = new sem_t*[num_processes];
    
    for(int i=1;i<num_processes;i++){
        sem_done[i] = sem_get("done", i);
        sem_ready[i] = sem_get("ready", i);
    }
    
#if GRAPHICS
    // initialize opengl graphics
    glutInit( &argc, argv );
    glutInitWindowSize( winsize, winsize );
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA );
    glutCreateWindow( PROGNAME );
    glClearColor( 1,1,1,1 );
    glutDisplayFunc( display_func );
    glutTimerFunc( 50, timer_func, 0 );
    glutMouseFunc( mouse_func );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable( GL_BLEND );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluOrtho2D( 0,1,0,1 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    glScalef( 1.0/Robot::worldsize, 1.0/Robot::worldsize, 1 ); 

    // define a display list for a robot body
    double h = 0.01;
    double w = 0.01;

    glPointSize( 4.0 );

    displaylist = glGenLists(1);
    glNewList( displaylist, GL_COMPILE );

    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

    glBegin( GL_POLYGON );
    glVertex2f( h/2.0, 0 );
    glColor3f( 0,0,0 ); // black
    glVertex2f( -h/2.0,  w/2.0 );
    glVertex2f( -h/2.0, -w/2.0 );
    glEnd();

    glEndList();
#endif // GRAPHICS
}

void Robot::UpdatePixels()
{
  double radians_per_pixel = fov / (double)pixel_count;
  
  double halfworld = worldsize * 0.5;

  // initialize pixels vector  
  FOR_EACH( it, pixels )
	{
	  it->range = Robot::range; // maximum range
      it->robot = NULL; // nothing detected
    }
    
  // find my location
  unsigned int count = sectors_pop[secpi(grid[gridi(id,0)],grid[gridi(id,1)])];
  unsigned int *ptr = sectors + secti(grid[gridi(id,0)],grid[gridi(id,1)],0);
  
  // check every robot in the world to see if it is detected
  for(unsigned int i=0;i<count;i++)
    {
      Robot* other = population[*ptr];
      ptr++;
      
      // discard if it's the same robot
      if( other == this )
				continue;
	  
      // discard if it's out of range. We put off computing the
      // hypotenuse as long as we can, as it's relatively expensive.
      	
      double dx = pose[posei(other->id,0)] - pose[posei(id,0)];

		// wrap around torus
		if( dx > halfworld )
		  dx -= worldsize;
		else if( dx < -halfworld )
		  dx += worldsize;
		
		if( fabs(dx) > Robot::range )
		  continue; // out of range
		
      double dy = pose[posei(other->id,1)] - pose[posei(id,1)];

		// wrap around torus
		if( dy > halfworld )
		  dy -= worldsize;
		else if( dy < -halfworld )
		  dy += worldsize;

		if( fabs(dy) > Robot::range )
		  continue; // out of range
		
      double range = hypot( dx, dy );
      if( range > Robot::range ) 
				continue; 
			
      // discard if it's out of field of view 
      double absolute_heading = atan2( dy, dx );
      double relative_heading = AngleNormalize((absolute_heading - pose[posei(id,2)]) );
      if( fabs(relative_heading) > fov/2.0   ) 
				continue; 
			
      // find which pixel it falls in 
      int pixel = floor( relative_heading / radians_per_pixel );
      pixel += pixel_count / 2;
      pixel %= pixel_count;

      assert( pixel >= 0 );
      assert( pixel < (int)pixel_count );

      // discard if we've seen something closer in this pixel already.
      if( pixels[pixel].range < range) 
		  continue;
		
      // if we made it here, we see this other robot in this pixel.
      pixels[pixel].range = range;
      pixels[pixel].robot = other;
    }	
}

void Robot::UpdatePose()
{
  // move according to the current speed 
  double dx = speed.v * cos(pose[posei(id,2)]);
  double dy = speed.v * sin(pose[posei(id,2)]);; 
  double da = speed.w;
  
  pose_next[posei(id,0)] = DistanceNormalize( pose[posei(id,0)] + dx );
  pose_next[posei(id,1)] = DistanceNormalize( pose[posei(id,1)] + dy );
  pose_next[posei(id,2)] = AngleNormalize( pose[posei(id,2)] + da );
  
  // find my location
  grid_next[gridi(id,0)] = floor(pose_next[posei(id,0)]/sector_width);
  grid_next[gridi(id,1)] = floor(pose_next[posei(id,1)]/sector_width);
}

void Robot::Worker()
{
    unsigned int per_process = (int) ceil(1.0*population_size / num_processes);
    
    unsigned int lower = spid * per_process;
    unsigned int upper = min(lower+per_process, population_size);
    
    //fprintf( stderr, "Process %d spawned to work on #%d to #%d.\n", spid, lower, upper );
    
    while(updates_max < 0 || updates <= updates_max){
        //fprintf( stderr, "Process %d has started working... Cycle #%d\n", spid, updates );
        
        for(unsigned int i=lower;i<upper;i++){
            Robot::population[i]->UpdatePixels();
            Robot::population[i]->Controller();
    	    Robot::population[i]->UpdatePose();
        }
        
        //fprintf( stderr, "Process %d is done working.\n", spid );
        
        double *tmpp;

        tmpp = pose;
        pose = pose_next;
        pose_next = tmpp;

        int *tmpg;

        tmpg = grid;
        grid = grid_next;
        grid_next = tmpg;
        
        if(spid == 0){
            for(int i=1;i<num_processes;i++){
                //fprintf( stderr, "Process %d is waiting for process %d...\n", spid, i );
                sem_wait(sem_done[i]);
            }
            
            //fprintf( stderr, "Process %d is synchronizing...\n", spid );
            
            Synchronize();
            
            //fprintf( stderr, "Process %d is done synchronizing.\n", spid );
            
            for(int i=1;i<num_processes;i++){
                //fprintf( stderr, "Process %d is waking up process %d...\n", spid, i );
                sem_post(sem_ready[i]);
            }
        }else{
            //fprintf( stderr, "Process %d is signaling for master...\n", spid );
            sem_post(sem_done[spid]);
            //fprintf( stderr, "Process %d is waiting for master...\n", spid );
            sem_wait(sem_ready[spid]);
        }
        
        //if(sem_trywait(enter_sem) == -1 && errno == EAGAIN){
        //    fprintf( stderr, "Process %d is synchronizing...\n", spid );
        //    
        //    Synchronize();
        //    
        //    fprintf( stderr, "Process %d is done synchronizing.\n", spid );
        //    
        //    for(int i=0;i<num_processes-1;i++){
        //        fprintf( stderr, "Process %d posted 1 wake up for enter.\n", spid );
        //        sem_post(enter_sem);
        //    }
        //    
        //    if(paused){
        //        fprintf( stderr, "Paused. Process %d is waiting...\n", spid );
        //        sem_wait(exit_sem);
        //    }else{
        //        fprintf( stderr, "Process %d is waking everyone up...\n", spid );
        //        
        //        for(int i=0;i<num_processes-1;i++){
        //            fprintf( stderr, "Process %d posted 1 wake up for exit.\n", spid );
        //            sem_post(exit_sem);
        //        }
        //        
        //        for(int i=0;i<num_processes-1;i++){
        //            fprintf( stderr, "Process %d used 1 sleep for safeguard.\n", spid );
        //            sem_wait(safe_guard);
        //        }
        //    }
        //}else{
        //    fprintf( stderr, "Process %d used 1 sleep for enter.\n", spid );
        //    fprintf( stderr, "Process %d is waiting...\n", spid );
        //    fprintf( stderr, "Process %d used 1 sleep for exit.\n", spid );
        //    sem_wait(exit_sem);
        //    fprintf( stderr, "Process %d posted 1 wake up for safeguard.\n", spid );
        //    sem_post(safe_guard);
        //}
        
        ++updates;
    }
    
    if( spid == 0 )
    {
      FOR_EACH( r, population )
  	  printf( "x %3f y %3f a %3f\n", pose[posei((*r)->id,0)], pose[posei((*r)->id,1)], pose[posei((*r)->id,2)]);
    }
    
    exit(0);
}

void Robot::Synchronize()
{  
  //fprintf( stderr, "Swapped pointers...\n" );
  
  for(int i=0;i<num_of_sectors;i++)
      for(int j=0;j<num_of_sectors;j++)
          sectors_pop[secpi(i,j)] = 0;
  
  //fprintf( stderr, "Cleared sectors...\n" );
  
  // Register each robot with the sectors
  FOR_EACH( r, population ){
    for(int i=grid[gridi((*r)->id,0)]-1;i<=grid[gridi((*r)->id,0)]+1;i++){
      for(int j=grid[gridi((*r)->id,1)]-1;j<=grid[gridi((*r)->id,1)]+1;j++){
          int g_x = (i+num_of_sectors)%num_of_sectors;
          int g_y = (j+num_of_sectors)%num_of_sectors;
          int k = sectors_pop[secpi(g_x,g_y)];
          
          sectors_pop[secpi(g_x,g_y)]++;
          sectors[secti(g_x,g_y,k)] = (*r)->id;
      }
    }
  }
  
  //fprintf( stderr, "Populated sectors...\n" );

  // possibly snooze to save CPU and slow things down
  if( sleep_msec > 0 )
	 usleep( sleep_msec * 1e3 );
}

// draw a robot
void Robot::Draw()
{
#if GRAPHICS
  glPushMatrix();
  glTranslatef( pose[posei(id,0)], pose[posei(id,1)], 0 );
  glRotatef( rtod(pose[posei(id,2)]), 0,0,1 );
  
  double r,g,b;
  
  if((grid[gridi(id,0)]%2+grid[gridi(id,1)]%2)%2 == 0){
      r = 0;
      g = 0;
      b = 1;
  }else{
      r = 1;
      g = 0;
      b = 0;
  }
  
  glColor3f(r,g,b);

  // draw the pre-compiled triangle for a body
  glCallList(displaylist);
  
  if( Robot::show_data )
	 {
		// render the sensors
		double rads_per_pixel = fov / (double)pixel_count;
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		
		for( unsigned int p=0; p<pixel_count; p++ )
		  {
				double angle = -fov/2.0 + (p+0.5) * rads_per_pixel;
				double dx1 = pixels[p].range * cos(angle+rads_per_pixel/2.0);
				double dy1 = pixels[p].range * sin(angle+rads_per_pixel/2.0);
				double dx2 = pixels[p].range * cos(angle-rads_per_pixel/2.0);
				double dy2 = pixels[p].range * sin(angle-rads_per_pixel/2.0);
				
				glColor4f( r,g,b, pixels[p].robot ? 0.2 : 0.05 );
				
				glBegin( GL_POLYGON );
				glVertex2f( 0,0 );
				glVertex2f( dx1, dy1 );
				glVertex2f( dx2, dy2 );
				glEnd();                  
		  }	  
	 }

  glPopMatrix();
#endif // GRAPHICS
}


void Robot::Run()
{    
    //fprintf( stderr, "Here I am.\n" );
    
    //fprintf( stderr, "Main thread is synchronizing...\n" );
        
    //working_threads = num_threads;
    
    //fprintf( stderr, "Main thread is done synchronizing.\n" );
    
    //fprintf( stderr, "Main thread is spawning %d threads...\n", num_threads );
    
    while(--spid>0){
        int childpid = fork();
        
        if(childpid >= 0){
            if(childpid == 0){
                break;
            }
        }else{
            fprintf( stderr, "Fork failed with error %d\n", errno );
            exit(-1);
        }
    }

    double *tmpp;

    tmpp = pose;
    pose = pose_next;
    pose_next = tmpp;

    int *tmpg;

    tmpg = grid;
    grid = grid_next;
    grid_next = tmpg;

    if(spid == 0){
        for(int i=1;i<num_processes;i++){
            //fprintf( stderr, "Process %d is waiting for process %d...\n", spid, i );
            sem_wait(sem_done[i]);
        }
        
        //fprintf( stderr, "Process %d is synchronizing...\n", spid );
        
        Synchronize();
        
        //fprintf( stderr, "Process %d is done synchronizing.\n", spid );
        
        for(int i=1;i<num_processes;i++){
            //fprintf( stderr, "Process %d is waking up process %d...\n", spid, i );
            sem_post(sem_ready[i]);
        }
    }else{
        //fprintf( stderr, "Process %d is signaling for master...\n", spid );
        sem_post(sem_done[spid]);
        //fprintf( stderr, "Process %d is waiting for master...\n", spid );
        sem_wait(sem_ready[spid]);
    }
        
    #if GRAPHICS
        glutMainLoop();
    #endif

    Worker();
}

/** Normalize a length to within 0 to worldsize. */
double Robot::DistanceNormalize( double d )
{
	while( d < 0 ) d += worldsize;
	while( d > worldsize ) d -= worldsize;
	return d; 
} 

/** Normalize an angle to within +/_ M_PI. */
double Robot::AngleNormalize( double a )
{
	while( a < -M_PI ) a += 2.0*M_PI;
	while( a >  M_PI ) a -= 2.0*M_PI;	 
	return a;
}
