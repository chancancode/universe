/****
   universe.c
   version 2
   Richard Vaughan  
****/

#include <assert.h>
#include <unistd.h>

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
std::vector< std::vector< std::vector<Robot*> > > Robot::sectors;
int Robot::num_of_sectors( 1 );
double Robot::sector_width( 0.1 );
int Robot::sector_search_radius( 1 );
unsigned int Robot::pixel_count( 8);
unsigned int Robot::sleep_msec( 50 );
uint64_t Robot::updates(0);
uint64_t Robot::updates_max( 0.0 ); 
bool Robot::paused( false );
int Robot::winsize( 600 );
int Robot::displaylist(0);
bool Robot::show_data( true );

double *Robot::pose_x;
double *Robot::pose_y;
double *Robot::pose_a;
double *Robot::speed_v;
double *Robot::speed_w;
double *Robot::color_r;
double *Robot::color_g;
double *Robot::color_b;
double *Robot::pose_next_x;
double *Robot::pose_next_y;
double *Robot::pose_next_a;
double *Robot::speed_next_v;
double *Robot::speed_next_w;
double *Robot::color_next_r;
double *Robot::color_next_g;
double *Robot::color_next_b;

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
 "  -z <int> : sets the number of milliseconds to sleep between updates.\n";

#if GRAPHICS
// GLUT callback functions ---------------------------------------------------

// update the world - this is called whenever GLUT runs out of events
// to process
static void idle_func( void )
{
  Robot::UpdateAll();
}

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
		Robot::paused = !Robot::paused;
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
  : pixels()
{
  // add myself to the static vector of all robots
  id = population.size();
  population.push_back( this );
  
  pose_next_x[id] = x;
  pose_next_y[id] = y;
  pose_next_a[id] = a;
  
  speed_next_v[id] = 0.0;
  speed_next_w[id] = 0.0;
  
  color_next_r[id] = r;
  color_next_g[id] = g;
  color_next_b[id] = b;

  pixels.resize( pixel_count );
}

void Robot::Init( int argc, char** argv )
{
  // seed the random number generator with the current time
  srand48(0);
	
  // parse arguments to configure Robot static members
	int c;
	while( ( c = getopt( argc, argv, "?dp:s:f:r:c:u:z:w:")) != -1 )
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
	
	// initialize the sectors
    num_of_sectors = floor(worldsize / range);
    sector_width = worldsize / num_of_sectors;
    sector_search_radius = ceil( range / sector_width);
    sectors = std::vector< std::vector< std::vector<Robot*> > >(num_of_sectors, std::vector< std::vector<Robot*> >(num_of_sectors));
	
	// initialize the buffers
    pose_x = new double[population_size];
    pose_y = new double[population_size];
    pose_a = new double[population_size];
    
    speed_v = new double[population_size];
    speed_w = new double[population_size];
    
    color_r = new double[population_size];
    color_g = new double[population_size];
    color_b = new double[population_size];
    
    pose_next_x = new double[population_size];
    pose_next_y = new double[population_size];
    pose_next_a = new double[population_size];
    
    speed_next_v = new double[population_size];
    speed_next_w = new double[population_size];
    
    color_next_r = new double[population_size];
    color_next_g = new double[population_size];
    color_next_b = new double[population_size];

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
    glutIdleFunc( idle_func );
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
  
  std::vector<Robot*> *sector = 0;
  
  // find my location
  int grid_x = floor(pose_x[id]/sector_width);
  int grid_y = floor(pose_y[id]/sector_width);
  
  sector = &sectors[grid_x][grid_y];
  
  // check every robot in the world to see if it is detected
  FOR_EACH( it, *sector )
    {
      Robot* other = *it;
      
      // discard if it's the same robot
      if( other == this )
				continue;
			
      // discard if it's out of range. We put off computing the
      // hypotenuse as long as we can, as it's relatively expensive.
		
      double dx = pose_x[other->id] - pose_x[id];

		// wrap around torus
		if( dx > halfworld )
		  dx -= worldsize;
		else if( dx < -halfworld )
		  dx += worldsize;
		
		if( fabs(dx) > Robot::range )
		  continue; // out of range
		
      double dy = pose_y[other->id] - pose_y[id];

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
      double relative_heading = AngleNormalize((absolute_heading - pose_a[id]) );
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
  double dx = speed_v[id] * cos(pose_a[id]);
  double dy = speed_v[id] * sin(pose_a[id]);; 
  double da = speed_w[id];
  
  pose_next_x[id] = DistanceNormalize( pose_x[id] + dx );
  pose_next_y[id] = DistanceNormalize( pose_y[id] + dy );
  pose_next_a[id] = AngleNormalize( pose_a[id] + da );
  
  // find my location
  int grid_x = floor(pose_next_x[id]/sector_width);
  int grid_y = floor(pose_next_y[id]/sector_width);
  
#if GRAPHICS
  // color myself
  if((grid_x%2+grid_y%2)%2 == 0){
      color_next_r[id] = 0;
      color_next_g[id] = 0;
      color_next_b[id] = 1;
  }else{
      color_next_r[id] = 1;
      color_next_g[id] = 0;
      color_next_b[id] = 0;
  }
#endif
  
  // Register with the sectors *move*
  for(int i=grid_x-sector_search_radius;i<=grid_x+sector_search_radius;i++)
    for(int j=grid_y-sector_search_radius;j<=grid_y+sector_search_radius;j++)
      sectors[(i+num_of_sectors)%num_of_sectors][(j+num_of_sectors)%num_of_sectors].push_back(this);
}

void Robot::UpdateAll()
{
  SwapBuffers();
  
  // if we've done enough updates, exit the program
  if( updates_max > 0 && updates > updates_max )
    {
        FOR_EACH( r, population )
			printf( "x %3f y %3f a %3f\n", pose_x[(*r)->id], pose_y[(*r)->id], pose_a[(*r)->id]);
        
        exit(1);
    }
  
  if( ! Robot::paused )
		{
            sectors = std::vector< std::vector< std::vector<Robot*> > >(num_of_sectors, std::vector< std::vector<Robot*> >(num_of_sectors));
            		    
			FOR_EACH( r, population )
				(*r)->UpdatePose();

			FOR_EACH( r, population )
				(*r)->UpdatePixels();

			FOR_EACH( r, population )
				(*r)->Controller();
		}

  ++updates;
  
  // possibly snooze to save CPU and slow things down
  if( sleep_msec > 0 )
	 usleep( sleep_msec * 1e3 );
}

// draw a robot
void Robot::Draw()
{
#if GRAPHICS
  glPushMatrix();
  glTranslatef( pose_x[id], pose_y[id], 0 );
  glRotatef( rtod(pose_a[id]), 0,0,1 );
  
	glColor3f( color_r[id], color_g[id], color_b[id] ); 

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
				
				glColor4f( color_r[id], color_g[id], color_b[id], pixels[p].robot ? 0.2 : 0.05 );
				
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
#if GRAPHICS
  glutMainLoop();
#else
  while( 1 )
    UpdateAll();
#endif
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

/** Swap the buffers **/
void Robot::SwapBuffers()
{
    double *temp;
    
    temp = pose_x;
    pose_x = pose_next_x;
    pose_next_x = temp;
    
    temp = pose_y;
    pose_y = pose_next_y;
    pose_next_y = temp;
    
    temp = pose_a;
    pose_a = pose_next_a;
    pose_next_a = temp;
    
    temp = speed_v;
    speed_v = speed_next_v;
    speed_next_v = temp;
    
    temp = speed_w;
    speed_w = speed_next_w;
    speed_next_w = temp;
    
    temp = color_r;
    color_r = color_next_r;
    color_next_r = temp;
    
    temp = color_g;
    color_g = color_next_g;
    color_next_g = temp;
    
    temp = color_b;
    color_b = color_next_b;
    color_next_b = temp;
}
