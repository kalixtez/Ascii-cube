#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32

#include <Windows.h>

#else

#include <sys/ioctl.h>
#include <unistd.h>

#endif

#define YELLOW ';'
#define BLUE 'b'
#define RED 'm'
#define GREEN 'o'
#define CYAN 'n'
#define BLACK '/'

typedef struct
{
	double X;
	double Y;
	double Z;
	double W;
} VERTEX;

typedef struct
{
	VERTEX v[3];
	char COLOUR; // "COLOUR" is a character, in this case.
} TRIANGLE;

double inc = 0.25 / 360.0 * 2 * 3.1416;
double rot_y = 0;
double rot_x = 0;

int viewport_height = 0;
int viewport_width = 0;
double far_plane = 10.0;
double near_plane = 1;

double right_plane = 1.0;
double left_plane = -1.0;

double up_plane = 0.5;
double bottom_plane = -0.5;

double *z_buffer;
char *framebuffer;

#ifdef _WIN32

HANDLE console_output;

#else

FILE *console_output;

#endif

void model_view(TRIANGLE *triangle)
{
	// First apply rotation.
	// Rotate about the Y axis.

	double tmp = cos(rot_y) * triangle->v[0].X + sin(rot_y) * triangle->v[0].Z;
	triangle->v[0].Z = -sin(rot_y) * triangle->v[0].X + cos(rot_y) * triangle->v[0].Z;
	triangle->v[0].X = tmp;

	tmp = cos(rot_y) * triangle->v[1].X + sin(rot_y) * triangle->v[1].Z;
	triangle->v[1].Z = -sin(rot_y) * triangle->v[1].X + cos(rot_y) * triangle->v[1].Z;
	triangle->v[1].X = tmp;

	tmp = cos(rot_y) * triangle->v[2].X + sin(rot_y) * triangle->v[2].Z;
	triangle->v[2].Z = -sin(rot_y) * triangle->v[2].X + cos(rot_y) * triangle->v[2].Z;
	triangle->v[2].X = tmp;

	// Then rotate about the X axis:

	tmp = cos(rot_x) * triangle->v[0].Y - sin(rot_x) * triangle->v[0].Z;
	triangle->v[0].Z = sin(rot_x) * triangle->v[0].Y + cos(rot_x) * triangle->v[0].Z;
	triangle->v[0].Y = tmp;

	tmp = cos(rot_x) * triangle->v[1].Y - sin(rot_x) * triangle->v[1].Z;
	triangle->v[1].Z = sin(rot_x) * triangle->v[1].Y + cos(rot_x) * triangle->v[1].Z;
	triangle->v[1].Y = tmp;

	tmp = cos(rot_x) * triangle->v[2].Y - sin(rot_x) * triangle->v[2].Z;
	triangle->v[2].Z = sin(rot_x) * triangle->v[2].Y + cos(rot_x) * triangle->v[2].Z;
	triangle->v[2].Y = tmp;

	// Lastly, translation.

	triangle->v[0].Z -= 3.5;
	triangle->v[1].Z -= 3.5;
	triangle->v[2].Z -= 3.5;

	rot_x += inc;
	if (rot_x >= 360)
	{
		rot_x = 0;
	}
	rot_y += inc;
	if (rot_y >= 360)
	{
		rot_y = 0;
	}
}

VERTEX projection_transform(VERTEX *vertex)
{
	VERTEX projected;
	projected.X = near_plane * vertex->X / right_plane;
	projected.Y = near_plane * vertex->Y / up_plane;

	projected.W = -vertex->Z;

	projected.Z = (far_plane + near_plane) / (far_plane - near_plane);
	projected.Z += (-2 * far_plane * near_plane) / ((far_plane - near_plane) * projected.W);

	projected.X = projected.X / projected.W;
	projected.Y = projected.Y / projected.W;

	return projected;
}

double edge_function(VERTEX v0, VERTEX v1, VERTEX p)
{
	return ((p.X - v0.X) * (v1.Y - v0.Y) - (p.Y - v0.Y) * (v1.X - v0.X));
}

void raster_triangle(TRIANGLE *triangle)
{
	VERTEX vertices[3];

	for (int vertex = 0; vertex < 3; vertex++)
	{
		vertices[vertex] = projection_transform(&triangle->v[vertex]);
		vertices[vertex].X = floor((vertices[vertex].X + 1) * viewport_width / 2);
		vertices[vertex].Y = floor(((vertices[vertex].Y) + 1) * viewport_height / 2);
	}

	double max_y; // Get maximum and minimum X and Y coords (triangle bounding box).
	double min_y;

	double max_x;
	double min_x;

	// correct winding order. if negative = okay
	double cross_z = (vertices[1].X - vertices[0].X) * (vertices[2].Y - vertices[0].Y) - (vertices[2].X - vertices[0].X) * (vertices[1].Y - vertices[0].Y);
	// cross_z is also the signed area of the parallelogram defined by the two vectors. (v1-v0; v2-v0)

	if (cross_z > 0) // clockwise
	{
		VERTEX v = vertices[1];
		vertices[1] = vertices[2];
		vertices[2] = v;
		cross_z *= -1;
	}

	if (vertices[0].Y <= vertices[1].Y)
	{
		max_y = vertices[1].Y;
		min_y = vertices[0].Y;
	}

	else
	{
		max_y = vertices[0].Y;
		min_y = vertices[1].Y;
	}

	if (max_y <= vertices[2].Y)
		max_y = vertices[2].Y;
	if (min_y >= vertices[2].Y)
		min_y = vertices[2].Y;

	if (vertices[0].X <= vertices[1].X)
	{
		max_x = vertices[1].X;
		min_x = vertices[0].X;
	}

	else
	{
		max_x = vertices[0].X;
		min_x = vertices[1].X;
	}

	if (max_x <= vertices[2].X)
		max_x = vertices[2].X;
	if (min_x >= vertices[2].X)
		min_x = vertices[2].X;

	for (int y_index = (int)min_y; y_index < max_y; y_index++)
	{
		for (int x_index = (int)min_x; x_index < max_x; x_index++)
		{
			if (0 <= x_index && x_index < viewport_width && 0 <= y_index && y_index < viewport_height)
			{

				VERTEX p = {x_index, y_index, 0, 0};
				// barycentric coordinates associated with V0, V1 and V2 respectively.
				double alpha, beta, gamma;
				alpha = edge_function(vertices[1], vertices[2], p);
				beta = edge_function(vertices[2], vertices[0], p);
				gamma = edge_function(vertices[0], vertices[1], p);

				if (alpha >= 0 && beta >= 0 && gamma >= 0)
				{
					// Implement z-buffering ()
					// compute barycentric coords by dividing by the signed area
					alpha /= cross_z;
					beta /= cross_z;
					gamma /= cross_z;

					// now interpolate z' (projected.Z) which is a function of 1/z, which at the same time can be linearly interpolated using screen-space coords.
					double interpolated = 1 / (alpha * vertices[0].Z + beta * vertices[1].Z + gamma * vertices[2].Z);
					if (interpolated <= z_buffer[y_index * viewport_width + x_index])
					{
						framebuffer[y_index * viewport_width + x_index] = triangle->COLOUR;
						z_buffer[y_index * viewport_width + x_index] = interpolated;
					}
					// putchar(triangle->COLOUR);
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
#ifdef _WIN32

	console_output = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO screen_info;
	GetConsoleScreenBufferInfo(console_output, &screen_info);

	viewport_width = screen_info.dwMaximumWindowSize.X;
	viewport_height = screen_info.dwMaximumWindowSize.Y;

#else

	console_output = stdout;

	struct winsize screen_info;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &screen_info);

	viewport_width = screen_info.ws_col;
	viewport_height = screen_info.ws_row;

#endif

	printf("%d     %d", viewport_width, viewport_height);

	getchar();

	z_buffer = (double *)malloc(sizeof(double) * viewport_width * viewport_height);
	framebuffer = (char *)malloc(sizeof(char) * viewport_width * viewport_height);

	for (int idx = 0; idx < viewport_width * viewport_height; idx++)
		z_buffer[idx] = 1.0;

	for (int idx = 0; idx < viewport_width * viewport_height; idx++)
		framebuffer[idx] = ' ';

	TRIANGLE back_0 = {.v = {{-1.0, 1.0, -1.0}, {-1.0, -1.0, -1.0}, {1.0, 1.0, -1.0}}, .COLOUR = YELLOW};
	TRIANGLE back_1 = {.v = {{1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0}, {1.0, 1.0, -1.0}}, .COLOUR = YELLOW};

	TRIANGLE front_0 = {.v = {{-1.0, 1.0, 1.0}, {-1.0, -1.0, 1.0}, {1.0, 1.0, 1.0}}, .COLOUR = RED};
	TRIANGLE front_1 = {.v = {{1.0, -1.0, 1.0}, {-1.0, -1.0, 1.0}, {1.0, 1.0, 1.0}}, .COLOUR = RED};

	TRIANGLE bottom_0 = {.v = {{-1.0, -1.0, 1.0}, {-1.0, -1.0, -1.0}, {1.0, -1.0, 1.0}}, .COLOUR = BLUE};
	TRIANGLE bottom_1 = {.v = {{1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0}, {1.0, -1.0, 1.0}}, .COLOUR = BLUE};

	TRIANGLE up_0 = {.v = {{-1.0, 1.0, 1.0}, {-1.0, 1.0, -1.0}, {1.0, 1.0, 1.0}}, .COLOUR = GREEN};
	TRIANGLE up_1 = {.v = {{1.0, 1.0, -1.0}, {-1.0, 1.0, -1.0}, {1.0, 1.0, 1.0}}, .COLOUR = GREEN};

	TRIANGLE left_0 = {.v = {{-1.0, 1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0, 1.0, 1.0}}, .COLOUR = BLACK};
	TRIANGLE left_1 = {.v = {{-1.0, -1.0, -1.0}, {-1.0, -1.0, 1.0}, {-1.0, 1.0, 1.0}}, .COLOUR = BLACK};

	TRIANGLE right_0 = {.v = {{1.0, 1.0, -1.0}, {1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}}, .COLOUR = CYAN};
	TRIANGLE right_1 = {.v = {{1.0, -1.0, -1.0}, {1.0, -1.0, 1.0}, {1.0, 1.0, 1.0}}, .COLOUR = CYAN};

	TRIANGLE cube[6][2] = {{back_0, back_1}, {front_0, front_1}, {bottom_0, bottom_1}, {up_0, up_1}, {left_0, left_1}, {right_0, right_1}};

	// printf("size x: %d\nsize y: %d", screen_info.dwMaximumWindowSize.X, screen_info.dwMaximumWindowSize.Y);

#ifdef _WIN32

	DWORD written = 0;

#endif

	int total_res = viewport_width * viewport_height;

	while (1)
	{
		// Write the sequence for clearing the display.

		// system("cls");

		for (int tidx = 0; tidx < 6; tidx++)
		{
			TRIANGLE tmp1 = cube[tidx][0];
			TRIANGLE tmp2 = cube[tidx][1];

			model_view(&tmp1);
			model_view(&tmp2);

			raster_triangle(&tmp1);
			raster_triangle(&tmp2);
		}

		framebuffer[viewport_width * viewport_height - 1] = '\0';

#ifdef _WIN32
		WriteConsoleOutputCharacter(console_output, framebuffer, viewport_height * viewport_width, (COORD){0, 0}, &written);

#else

		fprintf(console_output, framebuffer, total_res);

#endif

		for (int idx = 0; idx < total_res; idx++)
			framebuffer[idx] = ' ';
		for (int idx = 0; idx < total_res; idx++)
			z_buffer[idx] = 1.0;

#ifdef _WIN32
		Sleep(25);
#else
		usleep(25000);
		printf("\033[0;0f");
#endif
	}

	free(z_buffer);
}
