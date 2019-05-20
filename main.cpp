#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <SDL.h>
#include "GL/gl.h"

#define DIM_X 10
#define DIM_Y 10
#define MS_PER_UPDATE 16

class Point {
public:
	float x, y;

	Point(float x, float y) : x(x), y(y) {}
	~Point() {}

	Point* dup() { return new Point(x,y);}
	void translate(float dx, float dy) {x += dx; y += dy;}
};
class Line {
public:
	Point *a, *b;
	Line* next;

	Line (Point* a, Point* b) : a(a), b(b), next(0) {}
	~Line () {
		delete a;
		delete b;
	}

	Point* intersect(Line* that) {
		float den, t;

		den = (this->a->x-this->b->x)*(that->a->y-that->b->y)
		     -(this->a->y-this->b->y)*(that->a->x-that->b->x);
		if (den == 0) return 0; // parallel

		t   = (this->a->x-that->a->x)*(that->a->y-that->b->y)
		     -(this->a->y-that->a->y)*(that->a->x-that->b->x);
		t = t / den;
		if (t < 0 || 1 < t) return 0; // segments do not intersect

		return new Point( this->a->x + t*(this->b->x-this->a->x) ,
						  this->a->y + t*(this->b->y-this->a->y) );
	}
	Line* split(Point* s) {
		Line* l = new Line(s,b);
		b = s->dup();
		return l;
	}
	Line* try_split(Line* that) {
		Point* temp = intersect(that);
		return temp?split(temp):0;
	}
	void translate(float dx, float dy) {
		a->translate(dx,dy);
		b->translate(dx,dy);
	}
	Line* dup() {
		return new Line(a->dup(),b->dup());
	}
	float midpoint_theta(float xx, float yy) {
		float x = (a->x + b->x)/2;
		float y = (a->y + b->y)/2;
		return atan2(y-yy,x-xx);
	}
};
class Box {
private:
	int cycle = -1;
	Line* infront = 0;
	Line* infront_end;
	Line* behind = 0;
	Line* behind_end;
	float dx, dy;
	float cx, cy;
	Line *brdr[4];
	bool switch_lines_center = false;
	int cycle_length = 10;
	float spd = 0.02;

public:
	Box() {
		// helper lines to get rid of lines out of view
		// bigger than view because lines can be pushed back onto view
		brdr[0] = new Line( new Point(-10.*(DIM_X), 1.5*(DIM_Y)),
							new Point( 10.*(DIM_X), 1.5*(DIM_Y))); // top
		brdr[1] = new Line( new Point(-10.*(DIM_X),-1.5*(DIM_Y)),
							new Point( 10.*(DIM_X),-1.5*(DIM_Y))); // bottom
		brdr[2] = new Line( new Point(-1.5*(DIM_X),-10.*(DIM_Y)),
							new Point(-1.5*(DIM_X), 10.*(DIM_Y))); // left
		brdr[3] = new Line( new Point( 1.5*(DIM_X),-10.*(DIM_Y)),
							new Point( 1.5*(DIM_X), 10.*(DIM_Y))); // right
	}
	~Box() {
		for (int b=0; b<4; b++) delete brdr[b];
		Line *look, *next;
		look = infront;
		while (look) {
			next = look->next;
			delete look;
			look = next;
		}
		look = behind;
		while (look) {
			next = look->next;
			delete look;
			look = next;
		}
	}

	void split(int i) { if (i < 0) cycle_length-=5; else cycle_length+=5;
						if (cycle_length < 5) cycle_length =5;}
	void speed(int i) { if (i < 0) spd /= 2; else spd *= 2; }
	void center_toggle() {switch_lines_center = !switch_lines_center;}
	void report() {printf("spd[%f] period[%d] center[%d]\n",
						spd,cycle_length,switch_lines_center);}
	void count_lines() {
		int i = 0, j = 0;
		Line* look;
		for (look = infront; look; look = look->next, i++);
		for (look = behind ; look; look = look->next, j++);
		printf("count[%d]+[%d]=[%d]\n",i,j,i+j);
	}
	bool line_still_visible(Line* line) {
		Line* temp;
		float buf = 1.6;
		for (int b=0; b<4; b++) {
			// does any part of line segment cross the safety box
			temp = line->try_split(brdr[b]);
			if (!temp) continue;

			// determine line (a,s) or temp (s,b) to keep (desired side of box)
			if (  ( b==0 && temp->b->y <  (DIM_Y) ) // temp is below top
				||( b==1 && temp->b->y > -(DIM_Y) ) // temp is above bottom
				||( b==2 && temp->b->x > -(DIM_X) ) // temp is right of left
				||( b==3 && temp->b->x <  (DIM_X) ) // temp is left of right
				) {
				delete line->a; line->a = temp->a->dup();
				delete line->b; line->b = temp->b->dup();
			}
			delete temp;
		}
		// remainder either wholly in box or outside, only check one point
		// false positives will be caught as they're pushed further outside box
		return -DIM_Y*buf < line->a->y && line->a->y < DIM_Y*buf &&
			   -DIM_X*buf < line->a->x && line->a->x < DIM_X*buf;
	}
	void tick() {

		Line* new_line;
		Line* look;
		Line* temp;
		Line* all;
		float x, y, theta, theta2;

		// advance counter
		if (++cycle > cycle_length) cycle = 0;

		// new cycle, create new split
		if (cycle == 0) {
			// create new split that intersects a point

			// it is useful to have a reference point and then a rotation
			// for the math when determining line pos w.r.t. split line, do so
			theta = (rand() % 31415) / 10000.;
			if (switch_lines_center) {
				cx = 0;
				cy = 0;
			} else {
				cx = rand() % (2 * DIM_X) - DIM_X;
				cy = rand() % (2 * DIM_Y) - DIM_Y;
			}
			x = (DIM_X+DIM_Y)*2 * cos(theta);
			y = (DIM_X+DIM_Y)*2 * sin(theta);
			new_line = new Line(new Point(cx+x,cy+y),new Point(cx-x,cy-y));

			// calculate the perpindicular, movement of 'infront' lines
			theta2 = theta + 3.14159/2;
			dx = spd * cos(theta2);
			dy = spd * sin(theta2);

			// queue all existing lines into all
			all = 0;
			if (infront) {
				all = infront;
				infront_end->next = behind;
			}

			// split all lines that are newly intersected
			for (look = all; look; look = look->next) {
				// queue (and skip) new lines that are generated
				if ( (temp = look->try_split(new_line)) ) {
					temp->next = look->next;
					look->next = temp;
					look = temp;
				}
			}

			// clip and delete lines as appropriate
			temp = 0;
			look = all;
			while (look) {
				// delete line if not visible for whatever reason
				if (!line_still_visible(look)) {
					if (look == all) {
						temp = look->next;
						delete look;
						all = temp;
						look = temp;
					} else {
						temp->next = look->next;
						delete look;
						look = temp->next;
					}
				} else {
					temp = look;
					look = look->next;
				}
			}

			// assign all lines to either of infront or behind
			infront = new_line;
			infront_end = infront;
			behind = new_line->dup();
			behind_end = behind;
			for (look = all; look; look = look->next) {
				// infront iff midpoint within one +radian of new_line's angle
				theta2 = look->midpoint_theta(cx,cy);
				if (theta2 < 0) theta2 += 2 * 3.14159;
				theta2 -= theta;
				if (0 <= theta2 && theta2 < 3.14159) {
					infront_end->next = look;
					infront_end = look;
				} else {
					behind_end->next = look;
					behind_end = look;
				}
			}
			// properly terminate lists
			infront_end->next = 0;
			behind_end->next = 0;
		}

		// push line segments
		for (look = infront; look; look = look->next) look->translate( dx, dy);
		for (look = behind;  look; look = look->next) look->translate(-dx,-dy);
	}
	void render() {
		Line* look;
		glColor3f(1,1,1);
		glLineWidth(2);
		glBegin(GL_LINES);
		for (look=infront; look; look=look->next) {
			glVertex2f(look->a->x,look->a->y);
			glVertex2f(look->b->x,look->b->y);
		}
		for (look=behind; look; look=look->next) {
			glVertex2f(look->a->x,look->a->y);
			glVertex2f(look->b->x,look->b->y);
		}
		glEnd();
	}
};

int main(int argc, char* args[]) {
	srand(time(0));

	int current, elapsed, previous, lag;

	SDL_Window* win;
	SDL_Event* event;

	SDL_Init(SDL_INIT_EVERYTHING);

	win = SDL_CreateWindow(
			"Endless Splits",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			DIM_X * 2 * 50, DIM_Y * 2 * 50,
			SDL_WINDOW_OPENGL);
	SDL_GL_CreateContext(win);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_LINE_SMOOTH);

	event = new SDL_Event();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-(DIM_X),(DIM_X),-(DIM_Y),(DIM_Y),1,-1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();

	glClearColor(0,0,0,0);

	Box box;
	box.report();

	bool quit = false;
	bool pause = false;
	previous = SDL_GetTicks();
	lag = 0;
	while (!quit) {
		current = SDL_GetTicks();
		elapsed = current - previous;
		previous = current;
		lag += elapsed;

		while(SDL_PollEvent(event)) {
			switch(event->type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				switch(event->key.keysym.sym) {
				case SDLK_ESCAPE:
					quit = true;
					break;
				case SDLK_SPACE:
					pause = !pause;
					break;
				case SDLK_LEFT:
					box.split(-1);
					break;
				case SDLK_RIGHT:
					box.split(1);
					break;
				case SDLK_UP:
					box.speed(1);
					break;
				case SDLK_DOWN:
					box.speed(-1);
					break;
				case SDLK_RCTRL:
				case SDLK_LCTRL:
					box.center_toggle();
					break;
				case SDLK_c:
					box.count_lines();
					break;
				}
				box.report();
				break;
			}
		}

		while (MS_PER_UPDATE <= lag) {
			if (!pause) box.tick();
			lag -= MS_PER_UPDATE;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		box.render();

		SDL_GL_SwapWindow(win);
	}

	delete event;
	SDL_DestroyWindow(win);
	SDL_Quit();

	return 0;
}
