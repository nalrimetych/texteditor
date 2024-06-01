#define SDL_MAIN_HANDLED

#include <iostream>
#include <fstream>
#include <string>
#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <cstring>
#include <algorithm>
#include <list>
#include <iterator>


#define NUM_GLYPHS 128
#define FONT_SIZE 16
#define FONT_TEXTURE_SIZE 512

enum {
	FONT_ARIAL,
	FONT_CONSOLA,
	FONT_MAX
};

int m_window_width = 400, m_window_height = 700;

static TTF_Font* fonts[FONT_MAX];
static SDL_Rect glyphs[FONT_MAX][NUM_GLYPHS];
static SDL_Texture *fontTextures[FONT_MAX];

SDL_Renderer *renderer;
SDL_Color white = {255,255,255};
int column=0, longestLine=1;

class gap_buffer {
public:
	char buffer[1000];
	int gap_size = 1;
	int gap_left = 0;
	int gap_right = gap_size - gap_left - 1;
	int sizeofbuffer = 1;
	int digitlen=0;


	gap_buffer(std::string firstinput = "") {
		insert(firstinput, 0);
	}

	void grow(int k, int position) {
    // Dynamically allocate array 'a' based on the required size
	    char* a = new char[sizeofbuffer - position];

	    // Copy the part of the buffer that will be shifted
	    for (int i = position; i < sizeofbuffer; i++)
	        a[i - position] = buffer[i];

	    // Insert spaces in the buffer to create the gap
	    for (int i = 0; i < k; i++)
	        buffer[i + position] = ' ';

	    // Copy back the shifted part of the buffer
	    for (int i = 0; i < sizeofbuffer - position; i++)
	        buffer[position + k + i] = a[i];

	    // Clean up dynamically allocated memory
	    delete[] a;

	    // Update buffer size and gap positions
	    sizeofbuffer += k;
	    gap_right += k;
	}

	void left(int position) {
		while(position < gap_left) {
			gap_left--;
			gap_right--;
			buffer[gap_right+1] = buffer[gap_left];
			buffer[gap_left] = ' ';
		}
	}

	void right(int position) {
		while(position > gap_left) {
			gap_left++;
			gap_right++;
			buffer[gap_left-1] = buffer[gap_right];
			buffer[gap_right] = ' ';
		}
	}

	void move_cursor(int position) {
		if(position < gap_left)
			left(position);
		else
			right(position);
	}

	void insert(std::string input, int position) {
		if(input == "\t")
			input = "    ";

		int len = input.length();
		int i = 0;

		if(position != gap_left)
			move_cursor(position);

		while(i < len) {
			if(gap_right == gap_left)
				grow(10, position);

			buffer[gap_left] = input[i];
			gap_left++;
			i++;
			position++;
			digitlen++;
		}
	}
};
std::list<gap_buffer>::iterator row;



static void initFont(int fontType, char *filename) {
	SDL_Surface *surface, *text;
	SDL_Rect dest;
	int i;
	char c[2];
	SDL_Rect *g;

	memset(&glyphs[fontType], 0, sizeof(SDL_Rect) * NUM_GLYPHS);

	fonts[fontType] = TTF_OpenFont(filename, FONT_SIZE);

	surface = SDL_CreateRGBSurface(0, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE, 32, 0,0,0, 0xff);

	SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGBA(surface->format, 0,0,0,0));

	dest.x = dest.y = 0;

	for(i = 32; i < 127; i++) {
		c[0] = i;
		c[1] = 0;

		text = TTF_RenderUTF8_Blended(fonts[fontType], c, white);

		TTF_SizeText(fonts[fontType], c, &dest.w, &dest.h);

		if(dest.x + dest.w >= FONT_TEXTURE_SIZE) {
			dest.x = 0;
			dest.y += dest.h+1;
			if(dest.y + dest.h >= FONT_TEXTURE_SIZE) {
				SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, "Out of glyph space in %dx%d font atlas texture map.", FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE);
				return;
			}
		}

		SDL_BlitSurface(text, NULL, surface, &dest);

		g = &glyphs[fontType][i];

		g->x = dest.x;
		g->y = dest.y;
		g->w = dest.w;
		g->h = dest.h;

		SDL_FreeSurface(text);

		dest.x += dest.w;
	}

	fontTextures[fontType] = SDL_CreateTextureFromSurface(renderer, surface);
}


void initFonts(void) {
    initFont(FONT_ARIAL, "C:/Users/admin/Desktop/texteditor/fonts/arial.ttf");

    initFont(FONT_CONSOLA, "C:/Users/admin/Desktop/texteditor/fonts/consola.ttf");
}


void drawText(gap_buffer line, int x, int y, int r, int g, int b, int fontType, int column) {
	int i, character;
	SDL_Rect *glyph, dest;

	SDL_SetTextureColorMod(fontTextures[fontType], r, g, b);


	for(int i=0, index=0; i<line.digitlen; i++, index++) {
		if(index == line.gap_left)
			index = line.gap_right+1;
		char c = line.buffer[index];
		if(!c)
			break;
		character = line.buffer[index];
		glyph = &glyphs[fontType][character];
		dest.x = x-(column*glyph->w);
		dest.y = y;
		dest.w = glyph->w;
		dest.h = glyph->h;
		SDL_RenderCopy(renderer, fontTextures[fontType], glyph, &dest);
		x += glyph->w;
	}
}

std::list<gap_buffer> lines;
void openDaFile(char *filename) {
	if(!filename) {
		gap_buffer ex(" ");
		lines.push_back( ex );
	}

	std::ifstream doc(filename);

	char line[256];
	while(doc.getline(line, 256)) {
		std::string str = line;
		std::string addon="";
		for(char& c:str) {
			if(c=='\t')
				addon += "    ";
			else
				addon += c;
		}
		gap_buffer temp(addon);
		temp.digitlen = addon.length();
		longestLine = std::max(longestLine, (int)addon.length());
		lines.push_back(temp);
	}
	doc.close();
}



struct insertPoint {
	SDL_Rect rect;
	int desiredX, index;
	int insertRow;
	bool needed, atEnd;
} insPoint;

void updateInspoint() {
	insPoint.rect.y = (insPoint.insertRow * (glyphs[FONT_CONSOLA][0].h + 20));

	int linelen = std::next(row, insPoint.insertRow)->digitlen;


	insPoint.atEnd = true;

	int x=0;
	if(insPoint.desiredX < 0) {
		x = linelen * glyphs[FONT_CONSOLA][' '].w;
	} else {

		for(size_t i=0; i < linelen; i++) {
			int glyphW = glyphs[FONT_CONSOLA][' '].w;
			int glyphCenter = x + (glyphW / 2);

			if(insPoint.desiredX < glyphCenter) {
				insPoint.atEnd = false;
				break;
			}
			x += glyphW;
		}

	}
	insPoint.rect.x = x;
	insPoint.index = (x / (glyphs[FONT_CONSOLA][' '].w) + column);
}


void moveright(int dist = 1) {
	if(insPoint.atEnd) {
		insPoint.desiredX = 0;
		column = 0;
		if(insPoint.insertRow == (m_window_height / (glyphs[FONT_CONSOLA][0].h + 20))-1 ) {
			row++;
		} else
			insPoint.insertRow++;
		updateInspoint();

	} else {
		if(insPoint.rect.x + glyphs[FONT_CONSOLA][' '].w >= m_window_width)
			column += dist;
		else {
			insPoint.rect.x   += glyphs[FONT_CONSOLA][' '].w * dist;
			insPoint.desiredX += glyphs[FONT_CONSOLA][' '].w * dist;

		}
		insPoint.index += dist;
		if(insPoint.index == next(row, insPoint.insertRow)->digitlen)
			insPoint.atEnd = true;
	}
}
void moveleft() {
	if(insPoint.rect.x + (column*glyphs[FONT_CONSOLA][' '].w)==0 && !(row == lines.begin() && insPoint.insertRow == 0)) {
		insPoint.desiredX = -1;
		if(insPoint.insertRow == 0 ) {
			row--;
		} else
			insPoint.insertRow--;
		updateInspoint();

		if(insPoint.rect.x >= m_window_width) {
			int windowcolumns = m_window_width / glyphs[FONT_CONSOLA][' '].w;
			auto it = std::next(row, insPoint.insertRow);
			column = it->digitlen - windowcolumns;
		}

	}else {
		insPoint.atEnd = false;
		if(insPoint.rect.x == 0 && column > 0)
			column--;
		else if(insPoint.rect.x > 0){
			insPoint.rect.x   -= glyphs[FONT_CONSOLA][' '].w;
			insPoint.desiredX -= glyphs[FONT_CONSOLA][' '].w;
		}
		insPoint.index--;
	}
}

void save(char *myfile) {
	if(myfile==nullptr)
		myfile = "newfile.txt";
	std::ofstream output(myfile);
	for(auto it=row; it!=lines.end(); it++) {
		for(int i=0, index=0; i<it->digitlen; i++, index++) {
			if(index==it->gap_left)
				index = it->gap_right+1;
			output.put(it->buffer[index]);
		}
		output.put('\n');
	}	
}



int main(int argv, char** args) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("Failed to intialize the sdl2 library");
		SDL_Log(SDL_GetError());
		return -1;
	}
	if(TTF_Init() < 0) {
		SDL_Log("Failed to intialize the sdl2_ttf library",SDL_GetError());
		return -1;
	}

	SDL_Window *window = SDL_CreateWindow("SDL Window",
										  SDL_WINDOWPOS_CENTERED,
										  SDL_WINDOWPOS_CENTERED,
										  m_window_width, m_window_height,
										  SDL_WINDOW_RESIZABLE);
	renderer = SDL_CreateRenderer(window, -1, 0);

	if(!window) {
		SDL_Log("Failed to create window",SDL_GetError());
		return -1;
	}


	initFonts();


	char* myfile = args[1];
	openDaFile(myfile);
	row = lines.begin();


	
	insPoint.needed = false;
	insPoint.atEnd = true;
	insPoint = {-2, -2, 2, glyphs[FONT_CONSOLA][0].h+20};
	insPoint.desiredX = -1, insPoint.index = -1;
	insPoint.insertRow = -1;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	unsigned int a = SDL_GetTicks();
	unsigned int b = SDL_GetTicks();
	double delta = 0;


	std::cout << "lush life" << std::endl;

	
	bool win_open = true;
	while(win_open) {
		a = SDL_GetTicks();
		delta = a - b;


		if(delta > 1000 / 60.0) {
			b = a;


			SDL_Event e;
			while(SDL_PollEvent(&e) > 0) {
				switch(e.type) {
					case SDL_QUIT:
						win_open = false;
						save(myfile);
						break;
					case SDL_WINDOWEVENT:
						if(e.window.event == SDL_WINDOWEVENT_RESIZED) {
							SDL_Log("resizing da window...");
							SDL_GetWindowSize(window, &m_window_width, &m_window_height);
						}
						break;
					case SDL_KEYDOWN:
						if(e.key.keysym.sym == SDLK_ESCAPE) {
							win_open = false;
							save(myfile);
							break;
						}


						if(insPoint.needed) {
							if(e.key.keysym.sym == SDLK_DOWN && next(row, insPoint.insertRow) != prev(lines.end())) {

								if(insPoint.desiredX < 0)
									insPoint.desiredX = insPoint.rect.x;

								if(insPoint.insertRow == (m_window_height / (glyphs[FONT_CONSOLA][0].h + 20))-1 ) {
									row++;

								} else
									insPoint.insertRow++;

								updateInspoint();

							}

							if(e.key.keysym.sym == SDLK_UP && next(row, insPoint.insertRow) != lines.begin()) {
								if(insPoint.desiredX < 0)
									insPoint.desiredX = insPoint.rect.x;

								if(insPoint.insertRow == 0) 
									row--;
								else
									insPoint.insertRow--;

								updateInspoint();
							
							}

							if(e.key.keysym.sym == SDLK_RIGHT) {
								moveright();
							}

							if(e.key.keysym.sym == SDLK_LEFT){
								moveleft();
							}


							auto it = std::next(row, insPoint.insertRow);
							gap_buffer newline(" ");
							switch(e.key.keysym.sym) {
								case SDLK_TAB:
									it->insert("\t", insPoint.index);
									insPoint.atEnd = false;
									moveright(4);
									insPoint.desiredX = insPoint.rect.x;
									break;
								case SDLK_SPACE:
									it->insert(" ", insPoint.index);
									insPoint.atEnd = false;
									moveright();
									insPoint.desiredX = insPoint.rect.x;
									break;
								case SDLK_BACKSPACE:
									if(insPoint.index == 0) {
										moveleft();
										break;
									}
									it->move_cursor(insPoint.index);
									it->buffer[it->gap_left] = ' ';
									it->gap_left--;
									it->digitlen--;
									insPoint.index--;
									insPoint.rect.x -= glyphs[FONT_CONSOLA][' '].w;
									break;
								case SDLK_RETURN:
									++it;
									lines.insert(it, newline);
									break;
								default:
									std::string key = SDL_GetKeyName(e.key.keysym.sym);
									if(key.length() > 1)
										break;
									it->insert(key, insPoint.index);
									insPoint.atEnd = false;
									moveright();
									insPoint.desiredX = insPoint.rect.x;
									break;
							};

						}

						break;
					case SDL_MOUSEBUTTONDOWN:
						if(e.button.button == SDL_BUTTON_LEFT) {
							insPoint.needed = true;
							int mouseX = e.button.x;
							int mouseY = e.button.y;

							int clickedRow = (mouseY / (glyphs[FONT_CONSOLA][0].h + 20));
							if(clickedRow > lines.size() - column - 1 )
								clickedRow = lines.size() - column - 1;


							auto it = row;
							advance(it, clickedRow);
							std::string line = it->buffer;
							int x = 0;
							for(int i=0; i < it->digitlen; i++) {
								SDL_Rect *glyph = &glyphs[FONT_CONSOLA][line[i]];
								int glyphCenter = x + glyph->w / 2;

								if(mouseX < glyphCenter) {
									insPoint.atEnd = false;
									break;
								}
								x += glyph->w;
							}
							insPoint.rect.x = x;
							insPoint.rect.y = clickedRow * (glyphs[FONT_CONSOLA][0].h + 20);
							insPoint.desiredX = x;
							insPoint.index = (x / glyphs[FONT_CONSOLA][' '].w) + column;
							insPoint.insertRow = clickedRow;

						}
						break;
				};
			}
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	        SDL_RenderClear(renderer);

	        int level = 0;
	        for(std::list<gap_buffer>::iterator i=row; i!=lines.end(); i++) {
				drawText( *i, 0, level, 255, 255, 255, FONT_CONSOLA, column); //gotta change the drawing proccess so that it will happen only when some changes are commited (inserting , selecting or sum shit)
				level += glyphs[FONT_CONSOLA][0].h + 20;
	        }
	        if(insPoint.needed) {
				SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	        	SDL_RenderFillRect(renderer, &insPoint.rect);
	        	SDL_RenderDrawRect(renderer, &insPoint.rect);
	        }

			SDL_RenderPresent(renderer);

		}


	}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	
	TTF_Quit();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return EXIT_SUCCESS;
}