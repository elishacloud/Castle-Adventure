#include "allegro.h"
#include "winalleg.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "data.h"

#define for if(false) {} else for
#define WIN32_LEAN_AND_MEAN

#define MAX_HP          200

#define MAX_ROOM        84
#define MAX_MONSTER     11
#define MAX_ITEM        22
#define MAX_ITEM_CARRY  6
#define TILESIZE        32

#define SLEEPTIME		50

#define UP          0
#define DOWN        1
#define LEFT        2
#define RIGHT       3

/*
 * I am using a bit operation for the player's items and
 * monster types. Every time I grab an Item I OR the number 
 * to player.item. In binary the last number is actually the first
 * bit. Orintg an integer Ors each bit if either integers has a 
 * one for the bit then the resulting bit is one.
 *
 * ie: LAMP         = (Integer) 1 = (Binary) 000000000001
 *     BOOK         = (Integer) 4 = (Binary) 000000000100
 *                                           ------------
 *    (LAMP | BOOK) = (Integer) 5 = (Binary) 000000000101
 *
 * ie: player.item=player.item | LAMP; add lamp to item
 *
 * when I want to know if a player has a certain item, I do
 * an AND operation. Anding an integer ANDs each bit if both
 * integers have one for the bit then the resulting bit is one. 
 * If either of the integers has a zero then the resulting bit 
 * is zero
 *
 * ie: the player has the following items
 * LAMP,BOOK,MAGIC_WAND,SWORD,KEY, and HELMET
 *
 *    (player.item & LAMP)
 *     player.item         000000010111101
 *     LAMP                000000000000001
 *                         ---------------
 *     player.item & LAMP  000000000000001 == LAMP
 *
 *    (player.item & SCEPTER)
 *     player.item         000000010111101
 *     SCEPTER             000000000000010
 *                         ---------------
 *     player.item & LAMP  000000000000000 == zero
 */
#define LAMP            1
#define SCEPTER         2
#define BOOK            4
#define MAGIC_WAND      8
#define SWORD           16
#define KEY             32
#define EYE_GLASSES     64
#define HELMET          128
#define WINE_FLASK      256
#define CRYSTAL_BALL    512 
#define NECKLACE        1024
#define HOLY_CROSS      2048
#define DIAMOND         4096
#define SILVER_BARS     8192
#define RUBYS           16384
#define JADE_FIGURINE   32768
#define HARP            65536
#define HOURGLASS       131072
#define LARGE_GEM       262144
#define GOLDBAR         524288
#define FANCY_GOBLET    1048576
#define CROWN           2097152

/*
 * Used for monsters
 */
#define INACTIVE    0
#define ACTIVE      1
#define DEAD        2

#define UGLY_OGRE1      1
#define UGLY_OGRE2      2
#define ANGRY_DEMON1    4
#define ANGRY_DEMON2    8
#define BIG_SPIDER      16
#define SMALL_SPIDER    32
#define BAT             64
#define SNAKE           128
#define VAMPIRE         256
#define FAIRY1          512
#define FAIRY2          1024

DWORD LastSystemTime = 0;
DWORD CurrentDelay = 0;

typedef struct ITEM
{
    char name[20];
    int type;
    int gfx;
    int y;
    int x;
    int room;
} ITEM;

typedef struct MONSTER
{
    char adj[10];
    char name[10];
    int type;
    int gfx;
    int y;
    int x;
    int hit;
    int hp;
    int mhp;
    int room;
    int dir;
    int frame;
    int state;
} MONSTER;

typedef struct PLAYER
{
    char name[20];
    int rm;
    int x;
    int y;
    int dir;
    int frame;
    int item;
    int itemcount;
    int wear;
    int hp;
    int bt;
} PLAYER;

typedef struct ROOM
{
    char data[19][26];
    char desc[5][26];
    char exits[6];
    int beento;
} ROOM;

typedef struct HISCORE
{
    char name[20];
    int score;
} HISCORE;

/*
 * Global Variables
 */
HISCORE hs;
ROOM    room[MAX_ROOM];
PLAYER  player;
ITEM    item[MAX_ITEM];
MONSTER monster[MAX_MONSTER];

char log1[80],log2[80];
int stair=0;
volatile int count=0;
bool gameover=false;

DATAFILE *datafile;
BITMAP *buffer;
FONT *myfont;
/*
 * End of Global Variables
 */


/*
 * Function Prototypes
 */
void inc_count();

void saveHighScore();
void loadHighScore();

//void saveRooms();
void loadRooms();

bool areSure(char *tx1,char *tx2);

void save_game();
void load_game();

void initMonsters();
void initItems();
void initPlayer();
int initAll();
void resetGame();

void deleteAll();

void drawRoom(BITMAP *bmp);
void drawPlayer(BITMAP *bmp);
void drawMonster(BITMAP *bmp);
void drawItem(BITMAP *bmp);
void drawWindow(BITMAP *bmp,int w,int h,int style);
void drawLog(BITMAP *bmp);
void drawKeys(BITMAP *bmp);
void drawScreen(BITMAP *bmp);
void drawAll();

int select_item(char *name);

void dropItem(int t);
void do_drop();

void useItem(int t);
void use_item();

void wearItem(int t);
void wear_item();

void lookItem(int t);
void look_item();

void do_intro();

int computeScore();
void do_end(int p);

void do_inventory();
void do_search();
void do_look();

void inc_frame(int d);
bool hit_wall(int q,int x, int y);
bool hit_monster(int x, int y);
void moveMonsters();
void move(int d);
void do_flood();
void check_special();

void save_monster(int r);
void load_monster(int r);
void change_room(int r);

int Random(int u);

int main();

/*
 * End of Function Prototypes
 */


/*
 * This is used to slow down the monsters' movements
 */
void inc_count()
{
    count=1;
}
END_OF_FUNCTION(inc_count);


// Random value function
int Random(int u)
{
    return(rand() % u);
}

/*
 * This saves the high score into a file
 */
void saveHighScore()
{
    PACKFILE *pfile;

    pfile=pack_fopen("hiscore.dat", "wp");

    for (int i=0; i<20; i++)
    {
        pack_putc(hs.name[i],pfile);
    }
    pack_iputl(hs.score, pfile);

    pack_fclose(pfile);
}

/*
 * This checks to see if the high score file is there
 * If it isn't then create a bogus high score with my name
 */
void loadHighScore()
{
    PACKFILE *pfile;

    if (exists("hiscore.dat"))
    {
    	pfile=pack_fopen("hiscore.dat", "rp");

        for (int i=0; i<20; i++)
        {
            hs.name[i]=pack_getc(pfile);
        }
        hs.score=pack_igetl(pfile);

    	pack_fclose(pfile);
    }
    else
    {
        strcpy_s(hs.name,"Sir Daniel");
        hs.score=121;
    }
}

/*
 * Used these two function the first time to save
 * the room data. We no longer need this, but I left it
 * to show you.
 *
void saveRooms()
{
    PACKFILE *pfile;

    pfile=pack_fopen("castle.dat", "wp");

    for (int i=0; i<MAX_ROOM; i++)
    {
        for (int j=0; j<19; j++)
        {
            for (int k=0; k<25; k++)
            {
                pack_putc(room[i].data[j][k],pfile);
            }
        }
        for (int j=0; j<5; j++)
        {
            for (int k=0; k<27; k++)
            {
                pack_putc(room[i].desc[j][k],pfile);
            }
        }
        for (int j=0; j<6; j++)
        {
            pack_putc(room[i].exits[j],pfile);
        }
    }

    pack_fclose(pfile);
}

void initRooms()
{
    for (int i=0; i<MAX_ROOM; i++)
    {
        for (int j=0; j<19; j++)
        {
            strcpy_s(room[i].data[j],data[i][j]);
        }
        for (int j=0; j<5; j++)
        {
            strcpy_s(room[i].desc[j],desc[i][j]);
        }
        for (int j=0; j<6; j++)
        {
            room[i].exits[j]=exits[i][j];
        }
    }
    saveRooms();
}
*/

/*
 * Loads the castle room data from file
 */
void loadRooms()
{
    PACKFILE *pfile;

    pfile=pack_fopen("castle.dat", "rp");

    for (int i=0; i<MAX_ROOM; i++)
    {
        room[i].beento=0;
        for (int j=0; j<19; j++)
        {
            for (int k=0; k<25; k++)
            {
                room[i].data[j][k]=pack_getc(pfile);
            }
        }
        for (int j=0; j<5; j++)
        {
            for (int k=0; k<27; k++)
            {
                room[i].desc[j][k]=pack_getc(pfile);
            }
        }
        for (int j=0; j<6; j++)
        {
            room[i].exits[j]=pack_getc(pfile);
        }
    }

    pack_fclose(pfile);
}

/*
 * Little function that I supply two Yes or No question strings
 * returns true if answered Y
 */
bool areSure(char *tx1,char *tx2)
{
    BITMAP *temp;
    int w,h;
    char text[2][60];

    strcpy_s(text[0],tx1);
    strcpy_s(text[1],tx2);
    w=0;

    // I like my windows to have that snug fit so only make them
    // big enough to hold the text

    for (int i=0; i<2; i++)
    {
        if (w<text_length(myfont,text[i]))
            w=text_length(myfont,text[i]);
    }

    w+=16;
    h=2*TILESIZE+72;

    temp=create_bitmap(w,h);
    drawWindow(temp,w,h,0);

    textout_centre(temp,myfont,"Are You Sure?",w/2,0,makecol(255,255,255));

    for (int i=0; i<2; i++)
    {
        textout_centre(temp,myfont,text[i],w/2,60+i*TILESIZE,makecol(255,255,255));
    }

    acquire_screen();
    draw_sprite(screen,temp,(SCREEN_W-w)/2,(SCREEN_H-h)/2);
    release_screen();

    while (!key[KEY_Y] && !key[KEY_N])
    {
    }

    if (key[KEY_Y]) return true;

    return false;
}

/*
 * Saves a game and asks if you want to overwrite a
 * saved file
 */
void save_game()
{
    PACKFILE *pfile;

    if (exists("castle.sav"))
    {
        if (areSure("Are You Sure You Want To Overwrite","Saved Game? <Y/N>"))
        {
            pfile=pack_fopen("castle.sav","wp");

            for (int i=0; i<MAX_ROOM; i++)
            {
                for (int j=0; j<19; j++)
                {
                    for (int k=0; k<25; k++)
                    {
                        pack_putc(room[i].data[j][k],pfile);
                    }
                }
            }
            for (int i=0; i<MAX_MONSTER; i++)
            {        
                pack_iputl(monster[i].x,pfile);
                pack_iputl(monster[i].y,pfile);
                pack_iputl(monster[i].state,pfile);
                pack_iputl(monster[i].frame,pfile);
                pack_iputl(monster[i].dir,pfile);
                pack_iputl(monster[i].hp,pfile);
            }
            for (int i=0; i<MAX_ITEM; i++)
            {        
                pack_iputl(item[i].x,pfile);
                pack_iputl(item[i].y,pfile);
                pack_iputl(item[i].room,pfile);
            }
        
            pack_iputl(player.dir,pfile);
            pack_iputl(player.frame,pfile);
            pack_iputl(player.hp,pfile);
            pack_iputl(player.item,pfile);
            pack_iputl(player.itemcount,pfile);
            pack_iputl(player.rm,pfile);
            pack_iputl(player.wear,pfile);
            pack_iputl(player.x,pfile);
            pack_iputl(player.y,pfile);
            pack_iputl(player.bt,pfile);
    
            for (int i=0; i<83; i++)
            {
                pack_iputl(room[i].beento,pfile);
            }        

            pack_fclose(pfile);

            strcpy_s(log1,"Game Saved Successful!");
            strcpy_s(log2,"");
        }
    }
}

/*
 * Loads a saved game and asks whether you want to quit
 * the current game
 */
void load_game()
{
    PACKFILE *pfile;

    if (exists("castle.sav"))
    {
        if (areSure("Are You Sure You Want To Load A Saved Game","This Will End The Current Game? <Y/N>"))
        {
            pfile=pack_fopen("castle.sav","rp");

            for (int i=0; i<MAX_ROOM; i++)
            {
                for (int j=0; j<19; j++)
                {
                    for (int k=0; k<25; k++)
                    {
                        room[i].data[j][k]=pack_getc(pfile);
                    }
                }
            }
    
            for (int i=0; i<MAX_MONSTER; i++)
            {        
                monster[i].x=pack_igetl(pfile);
                monster[i].y=pack_igetl(pfile);
                monster[i].state=pack_igetl(pfile);
                monster[i].frame=pack_igetl(pfile);
                monster[i].dir=pack_igetl(pfile);
                monster[i].hp=pack_igetl(pfile);
            }   
            for (int i=0; i<MAX_ITEM; i++)
            {        
                item[i].x=pack_igetl(pfile);
                item[i].y=pack_igetl(pfile);
                item[i].room=pack_igetl(pfile);
            }
        
            player.dir=pack_igetl(pfile);
            player.frame=pack_igetl(pfile);
            player.hp=pack_igetl(pfile);
            player.item=pack_igetl(pfile);
            player.itemcount=pack_igetl(pfile);
            player.rm=pack_igetl(pfile);
            player.wear=pack_igetl(pfile);
            player.x=pack_igetl(pfile);
            player.y=pack_igetl(pfile);
            player.bt=pack_igetl(pfile);

            for (int i=0; i<83; i++)
            {
                room[i].beento=pack_igetl(pfile);
            }
        

            pack_fclose(pfile);
            strcpy_s(log1,"Game Loaded Successful!");
            strcpy_s(log2,"");
        }
    }
    else
    {
        strcpy_s(log1,"There Is No Saved Game To Load!");
        strcpy_s(log2,"");
    }
}

/*
 * Init The Monster data
 */
void initMonsters()
{
    MONSTER mon[MAX_MONSTER]={
    {"Ugly","Ogre",UGLY_OGRE1,0,8,12,4,80,80,4,0,0,INACTIVE},
    {"Ugly","Ogre",UGLY_OGRE2,0,11,11,4,80,80,21,0,0,INACTIVE},
    {"Angry","Demon",ANGRY_DEMON1,1,7,12,5,120,120,14,0,0,INACTIVE},
    {"Angry","Demon",ANGRY_DEMON2,1,9,5,5,120,120,24,0,0,INACTIVE},
    {"Big","Spider",BIG_SPIDER,2,13,17,3,80,80,73,0,0,INACTIVE},
    {"Small","Spider",SMALL_SPIDER,3,4,16,1,50,50,70,0,0,INACTIVE},
    {"","Bat",BAT,4,4,17,2,40,40,68,0,0,INACTIVE},
    {"","Snake",SNAKE,5,13,16,3,50,50,17,0,0,INACTIVE},
    {"","Vampire",VAMPIRE,6,9,23,0,31,31,28,0,0,INACTIVE},
    {"","Fairy",FAIRY1,7,9,0,0,0,0,52,0,0,INACTIVE},
    {"","Fairy",FAIRY2,7,0,11,0,0,0,54,0,0,INACTIVE}};

    for (int i=0; i<MAX_MONSTER; i++)
    {
        strcpy_s(monster[i].adj,mon[i].adj);
        strcpy_s(monster[i].name,mon[i].name);
        monster[i].type     =mon[i].type;
        monster[i].gfx      =mon[i].gfx;
        monster[i].y        =mon[i].y;
        monster[i].x        =mon[i].x;
        monster[i].hit      =mon[i].hit;
        monster[i].hp       =mon[i].hp;
        monster[i].mhp      =mon[i].mhp;
        monster[i].room     =mon[i].room;
        monster[i].dir      =mon[i].dir;
        monster[i].frame    =mon[i].frame;
        monster[i].state    =mon[i].state;
    }
}

/*
 * Init All the Item data and place in rooms data
 */
void initItems()
{
ITEM it[MAX_ITEM]={
    {"Lamp",LAMP,0,14,19,17},
    {"Scepter",SCEPTER,1,10,12,83},
    {"Book",BOOK,2,219,219,158},
    {"Magic Wand",MAGIC_WAND,3,9,4,24},
    {"Sword",SWORD,4,5,11,12},
    {"Key",KEY,5,196,179,156},
    {"Eye Glasses",EYE_GLASSES,6,196,179,140},
    {"Helmet",HELMET,7,5,12,22},
    {"Wine Flask",WINE_FLASK,8,196,179,110},
    {"Crystal Ball",CRYSTAL_BALL,9,13,10,66},
    {"Necklace",NECKLACE,10,196,179,119},
    {"Holy Cross",HOLY_CROSS,11,13,15,37},
    {"Diamond",DIAMOND,12,10,13,57},
    {"Silver Bars",SILVER_BARS,13,8,7,26},
    {"Ruby",RUBYS,14,16,4,60},
    {"Jade Figurine",JADE_FIGURINE,15,10,13,29},
    {"Harp",HARP,16,8,16,65},
    {"Hourglass",HOURGLASS,17,14,6,20},
    {"Large Gem",LARGE_GEM,18,196,179,118},     
    {"Goldbar",GOLDBAR,19,14,23,74},                    
    {"Fancy Goblet",FANCY_GOBLET,20,14,6,8},            
    {"Crown",CROWN,21,5,9,14}};                         
 
    for (int i=0; i<MAX_ITEM; i++)
    {
        strcpy_s(item[i].name,it[i].name);
        item[i].type    =it[i].type;
        item[i].gfx     =it[i].gfx;
        item[i].y       =it[i].y;
        item[i].x       =it[i].x;
        item[i].room    =it[i].room;
        if (it[i].room<84)
        {
            room[it[i].room-1].data[it[i].y-1][it[i].x-1]='A'+it[i].gfx;
        }
    }
}

/*
 * Init the player data, and ask for a name
 */
void initPlayer()
{
    BITMAP *temp;
    int w,h;
    char ch,text[6][40]={"New Player","","Please Enter Your Name","Brave Knight!","",""},
        nm[20],buf[20];

    player.x=384;
    player.y=412;
    player.dir=DOWN;
    player.frame=4;
    player.item=0;
    player.itemcount=0;
    player.wear=0;
    player.rm=1;
    player.hp=MAX_HP;
    player.bt=1;

    w=0;
    
    for (int i=0; i<6; i++)
    {
        if (w<text_length(myfont,text[i]))
            w=text_length(myfont,text[i]);
    }

    w+=16;
    h=6*TILESIZE+16;

    temp=create_bitmap(w,h);

    drawAll();

    ch=0;
    strcpy_s(nm,"Sir ");
    clear_keybuf();

    while (ch!=13)
    {
        if (keypressed())
        {
            ch=(readkey() & 0xFF);
            if (ch==8 && strlen(nm)>4 )
            {
                strcpy_s(buf,"");
                strncat_s(buf, nm, strlen(nm)-1);
                strcpy_s(nm,buf);
            }

            if (strlen(nm)<15)
            {
                if ((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch==' '))
                {
                    sprintf_s(nm,"%s%c",nm,ch);
                }
            }
            if (ch==13 && strlen(nm)<5)
            {
                ch=0;
            }
        }

        drawWindow(temp,w,h,0);
    
        for (int i=0; i<6; i++)
        {
            textout_centre(temp,myfont,text[i],w/2,i*TILESIZE,makecol(255,255,255));
        }
        textout_centre(temp,myfont,nm,w/2,160,makecol(100,100,255));
        acquire_screen();
        draw_sprite(screen,temp,(SCREEN_W-w)/2,(SCREEN_H-h)/2);
        release_screen();
    }

    strcpy_s(player.name,nm);

    while (key[KEY_ENTER] || key[KEY_ENTER_PAD]) {}

    destroy_bitmap(temp);
}

/*
 * Initialize allegro and the datafile
 */
int initAll()
{
    allegro_init();
    install_keyboard();
    install_timer();
    install_mouse();
	
/*
    if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL) != 0)
    {
        allegro_message("Unable initialize sound module\n%s\n", allegro_error);
        return -1;
    }
*/
    set_color_depth(16);

    if (set_gfx_mode(GFX_AUTODETECT,1024,768,0,0)!=0)
    {
        set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
        allegro_message("Unable initialize graphics module\n%s\n", allegro_error);
        while (!key[KEY_ENTER]) {}
        return -1;
    }
	
    datafile=load_datafile("data.dat");

    myfont=(FONT*)datafile[MY_FONT].dat;

    buffer=create_bitmap(SCREEN_W,SCREEN_H);
    clear_to_color(buffer,makecol(0,0,0));

    text_mode(-1);

    LOCK_VARIABLE(count);
    LOCK_FUNCTION(inc_count);

    install_int(inc_count,45);

    srand(time(NULL));

    resetGame();

    loadHighScore();

    return 0;
}

/*
 * Resets all the Game data
 */
void resetGame()
{
    strcpy_s(log1,"");
    strcpy_s(log2,"");

    loadRooms();

    initMonsters();
    initItems();
    initPlayer();
}

/*
 * free all memory used and save high score
 */
void deleteAll()
{
    saveHighScore();
    destroy_bitmap(buffer);
    unload_datafile(datafile);
}

/*
 * draw the room data
 */
void drawRoom(BITMAP *bmp)
{
    char p,a,c;
    for (int i=0; i<24; i++)
    {
        for (int j=0; j<18; j++)
        {
            c=100;
            p=room[player.rm-1].data[j][i];
            a=TILE000+p-'a';
            if (p=='*')
            {
                a=BLANK;
            }
            if (p==' ' || p=='#')
            {
                a=FLOOR;
            }
            if (p>='A' && p<='V')
            {
                a=FLOOR;
                c=item[p-'A'].gfx;
            }

            blit((BITMAP*)datafile[a].dat,bmp,0,0,8+i*TILESIZE,8+j*TILESIZE,TILESIZE,TILESIZE);
            if (c!=100)
                draw_sprite(bmp,(BITMAP*)datafile[ITEM000+c].dat,8+i*TILESIZE,8+j*TILESIZE);
        }
    }
    for (int i=0; i<5; i++)
    {
        textout(bmp,myfont,room[player.rm-1].desc[i],8,584+i*(text_height(myfont)-6),makecol(255,255,255));
    }
}

/*
 * draw character to the buffer
 */
void drawPlayer(BITMAP *bmp)
{
    masked_blit((BITMAP*)datafile[CHARACTER].dat,bmp,0,player.frame*TILESIZE,8+player.x,8+player.y,TILESIZE,TILESIZE);
}

/*
 * draw the monster on the screen and the name of the monster
 * in the upper right hand corner
 */
void drawMonster(BITMAP *bmp)
{
    int mtype[8]={MON0000,MON1000,MON2000,MON3000,MON4000,MON5000,MON6000,MON7000};
    for (int i=0; i<MAX_MONSTER; i++)
    {
        if (monster[i].room==player.rm)
        {
            masked_blit((BITMAP*)datafile[mtype[monster[i].gfx]+monster[i].frame].dat,bmp,0,0,8+monster[i].x,8+monster[i].y,TILESIZE,TILESIZE);
            masked_blit((BITMAP*)datafile[mtype[monster[i].gfx]+4].dat,bmp,0,0,776,8,TILESIZE,TILESIZE);
            textprintf(bmp,myfont,808,8,makecol(255,255,255),"%s %s",monster[i].adj,monster[i].name);            
        }
    }
}

/*
 * If the Item is in the room I want to draw the name
 * in the upper right hand corner
 */
void drawItem(BITMAP *bmp)
{
    int k=0;
    for (int i=0; i<MAX_MONSTER; i++)
    {
        if (monster[i].room==player.rm)
        {
            k=1;
        }
    }
    for (int i=0; i<MAX_ITEM; i++)
    {
        if (item[i].room==player.rm && (player.item & item[i].type)==0)
        {
            masked_blit((BITMAP*)datafile[ITEM000+item[i].gfx].dat,bmp,0,0,786,8+k*TILESIZE,TILESIZE,TILESIZE);
            textout(bmp,myfont,item[i].name,818,8+k*TILESIZE,makecol(255,255,255));
            k++;
        }
    }
}

/*
 * A little function that draws two types of windows
 * black screen with outline or just outline
 */
void drawWindow(BITMAP *bmp,int w,int h,int style)
{

    if (style==0) // black window with white outline
    {
        clear_to_color(bmp,makecol(0,0,0));
        hline(bmp,1,0,w-1,makecol(255,255,255));
        vline(bmp,0,1,h-1,makecol(255,255,255));
        hline(bmp,1,h-1,w-1,makecol(255,255,255));
        vline(bmp,w-1,1,h-1,makecol(255,255,255));

        putpixel(bmp,1,1,makecol(255,255,255));
        putpixel(bmp,w-2,1,makecol(255,255,255));
        putpixel(bmp,w-2,h-2,makecol(255,255,255));
        putpixel(bmp,1,h-2,makecol(255,255,255));
    
        putpixel(bmp,0,0,makecol(255,0,255));
        putpixel(bmp,w-1,0,makecol(255,0,255));
        putpixel(bmp,w-1,h-1,makecol(255,0,255));
        putpixel(bmp,0,h-1,makecol(255,0,255));
    }
    if (style==1) // white outline only
    {
        hline(bmp,1,0,w-1,makecol(255,255,255));
        vline(bmp,0,1,h-1,makecol(255,255,255));
        hline(bmp,1,h-1,w-1,makecol(255,255,255));
        vline(bmp,w-1,1,h-1,makecol(255,255,255));

        putpixel(bmp,1,1,makecol(255,255,255));
        putpixel(bmp,w-2,1,makecol(255,255,255));
        putpixel(bmp,w-2,h-2,makecol(255,255,255));
        putpixel(bmp,1,h-2,makecol(255,255,255));
    
        putpixel(bmp,0,0,makecol(0,0,0));
        putpixel(bmp,w-1,0,makecol(0,0,0));
        putpixel(bmp,w-1,h-1,makecol(0,0,0));
        putpixel(bmp,0,h-1,makecol(0,0,0));
    }

}

/*
 * Used to select item for drop,look at,wear,etc
 */
int select_item(char *name)
{
    int w,h,k,p=0,x,y,t=0,r=-1;
    BITMAP *temp;

    if (player.itemcount==0)
    {
        w=200;
        h=66;
    }
    else
    {
        w=200;
        h=40+34*player.itemcount;
    }

    x=(SCREEN_W-w)/2;
    y=(SCREEN_H-h)/2;

    temp=create_bitmap(w,h);

    while (!key[KEY_ENTER] && !key[KEY_ENTER_PAD] &&!key[KEY_ESC] && !key[KEY_SPACE])
    {
        k=0;
        drawWindow(temp,w,h,0);

        textout_centre(temp,myfont,name,w/2,0,makecol(255,255,255));

        if (player.itemcount==0)
        {
            t=-1;
            rectfill(temp,2,24,w-3,24+TILESIZE,makecol(192,192,192));
            textout(temp,myfont,"Nothing",8,24,makecol(255,255,255));
        }
        else    
        {
            t=-1;
            for (int i=0; i<MAX_ITEM; i++)
            {
                if ((player.item & item[i].type)!=0)
                {
                    if (k==p) 
                    {
                        rectfill(temp,2,24+k*TILESIZE,w-3,24+k*TILESIZE+TILESIZE,makecol(192,192,192));
                        t=i;
                    }
               	    masked_blit((BITMAP*)datafile[ITEM000+item[i].gfx].dat,temp,0,0,8,24+k*TILESIZE,TILESIZE,TILESIZE);
                    textout(temp,myfont,item[i].name,40,24+k*TILESIZE,makecol(255,255,255));
                    k++;
                }
            }
        }

        if (key[KEY_UP] && p>0)
        {
            p--;
            while (key[KEY_UP]) {}
        }
        if (key[KEY_DOWN] && p<(player.itemcount-1))
        {
            p++;
            while (key[KEY_DOWN]) {}
        }

        draw_sprite(buffer,temp,x,y);
        drawScreen(buffer);
    }

    while (key[KEY_ESC]) {}

    if (key[KEY_SPACE] || key[KEY_ENTER] || key[KEY_ENTER_PAD] )
    {
        if (t>=0)
        {
            r=t;
        }
    }

    destroy_bitmap(temp);
    return r;
}

/*
 * drop the supplied item
 */
void dropItem(int t)
{
    int x,y,r;


    r=player.rm-1;
    if (player.dir==UP) 
    {
        x=(player.x)/TILESIZE;
        y=(player.y)/TILESIZE;
        if (room[r].data[y-1][x]==' ')
        {
            room[r].data[y-1][x]='A'+item[t].gfx;
            item[t].x=x;
            item[t].y=y-1;
            item[t].room=player.rm;
            player.item-=item[t].type;
            player.itemcount--;
            if ((player.wear & item[t].type)!=0)
            {
                player.wear-=item[t].type;
            }
        }
    }
    if (player.dir==DOWN)
    {
        x=(player.x+TILESIZE-1)/TILESIZE;
        y=(player.y+TILESIZE-1)/TILESIZE;
        if (room[r].data[y+1][x]==' ')
        {
            room[r].data[y+1][x]='A'+item[t].gfx;
            item[t].x=x;
            item[t].y=y+1;
            item[t].room=player.rm;
            player.item-=item[t].type;
            player.itemcount--;
            if ((player.wear & item[t].type)!=0)
            {
                player.wear-=item[t].type;
            }
        }
    }
    if (player.dir==LEFT)
    {
        x=(player.x)/TILESIZE;
        y=(player.y)/TILESIZE;
        if (room[r].data[y][x-1]==' ')
        {
            room[r].data[y][x-1]='A'+item[t].gfx;
            item[t].x=x-1;
            item[t].y=y;
            item[t].room=player.rm;
            player.item-=item[t].type;
            player.itemcount--;
            if ((player.wear & item[t].type)!=0)
            {
                player.wear-=item[t].type;
            }
        }
    }
    if (player.dir==RIGHT)
    {
        x=(player.x+TILESIZE-1)/TILESIZE;
        y=(player.y+TILESIZE-1)/TILESIZE;
        if (room[r].data[y][x+1]==' ')
        {
            room[r].data[y][x+1]='A'+item[t].gfx;
            item[t].x=x+1;
            item[t].y=y;
            item[t].room=player.rm;
            player.item-=item[t].type;
            player.itemcount--;
            if ((player.wear & item[t].type)!=0)
            {
                player.wear-=item[t].type;
            }
        }
    }
}

/*
 * Run the select Item function and supply it to drop item
 */
void do_drop()
{
    int t=select_item("Drop Item");
    if (t>=0) dropItem(t);
}

/*
 * uses the supplied item
 */
void useItem(int t)
{
    bool p=false;
    if (item[t].type==WINE_FLASK)
    {
        strcpy_s(log1,"After You Drink The Water From");
        strcpy_s(log2,"The Flask, You Feel Much Better!");
        player.hp=MAX_HP;
        p=true;
    }
    if (item[t].type==SCEPTER)
    {
        p=true;
        if (player.rm==1)
        {
            strcpy_s(log1,"As You Wave The Scepter, The Gate");
            strcpy_s(log2,"And Vanishes In A Puff Of Smoke!");
            strcpy_s(room[0].data[17],"cccccccccc    cccccccccc*");                    
        }
        else
        {
            strcpy_s(log1,"As You Wave The Scepter,");
            strcpy_s(log2,"Nothing Happens!");
        }
    }
    if (item[t].type==HOLY_CROSS)
    {
        if (player.rm==28)
        {
            p=true;
            monster[8].room=-1;
            strcpy_s(log1,"The Vampire Sees The Holy Cross");
            strcpy_s(log2,"And Disappears In A Puff Of Smoke!");
        }

    } 
    if (item[t].type==HARP)
    {
        p=true;
        strcpy_s(log1,"The Harp Makes	Beautiful Music!");
        strcpy_s(log2,"");
        if (player.rm==54 || player.rm==52)
        {
            monster[9].room=-1;
            monster[9].state=DEAD;
            monster[10].room=-1;
            monster[10].state=DEAD;

            strcpy_s(log2,"The Fairy Likes The Music And Leaves!");
        }
    }
    if (item[t].type==MAGIC_WAND)
    {
        p=true;
        if (player.rm==77)
        {
            strcpy_s(log1,"As You Wave The Magic Wand,");
            strcpy_s(log2,"A Door Opens In A Puff Of Smoke!");
            strcpy_s(room[76].data[17], "***cccccccc  cccccccccc**");            
        } 
        else
        {
            if (player.rm==67)
            {
                strcpy_s(log1,"As You Wave The Magic Wand,");
                strcpy_s(log2,"A Door Opens In A Puff Of Smoke!");
                strcpy_s(room[66].data[7],"cccccc            dcccccc");
                strcpy_s(room[66].data[8],"                         ");
                strcpy_s(room[66].data[9],"                         ");
                strcpy_s(room[66].data[10],"cccccc            dcccccc");
            } 
            else
            {
                strcpy_s(log1,"As You Wave The Magic Wand,");
                strcpy_s(log2,"Nothing Happens!");
            }
        }
    }

    if (!p)
    {
        strcpy_s(log1,"Don't You Feel Silly Waving");
        strcpy_s(log2,"That Thing Around!");
    }
}

/*
 * run the select item function and send it to useitem
 */
void use_item()
{
    int t=select_item("Use Item");
    if (t>=0) useItem(t);
}

/*
 * wear the supplied item
 */
void wearItem(int t)
{
    bool p=false;
    if (item[t].type==NECKLACE)
    {
        player.wear|=NECKLACE;
        strcpy_s(log1,"For Some Reason, You Feel");
        strcpy_s(log2,"Safe Wearing The Necklace!");
        p=true;
    }
    if (item[t].type==EYE_GLASSES)
    {
        player.wear|=EYE_GLASSES;
        strcpy_s(log1,"You Put On the Eye Glasses");
        strcpy_s(log2,"And You Can See Better!");
        p=true;
    }
    if (item[t].type==HELMET)
    {
        player.wear|=HELMET;
        strcpy_s(log1,"You Are Now Wearing");
        strcpy_s(log2,"The Helmet!");
        p=true;
    }
    if (!p)
    {
        strcpy_s(log1,"You Would Look Silly Wearing It!");
        strcpy_s(log2,"");
    }
}

/*
 * run select item function and send it to wearItem if you aren't
 * allready wearing it
 */
void wear_item()
{
    int t=select_item("Wear Item");
    if (t>=0)
    {
        if ((player.wear & item[t].type)==0)
        {
            wearItem(t);
        }
        else
        {
            strcpy_s(log1,"You Are Allready Wearing It!");
            strcpy_s(log2,"");
        }
    }
}

/*
 * Gives a description of the item supplied
 */
void lookItem(int t)
{
    switch (item[t].type)
    {
    case LAMP:
        {
            strcpy_s(log1,"The Lamp Is Magically Lit");
            strcpy_s(log2,"");
        } break; 
    case HARP:
        {
            strcpy_s(log1,"A Solid Gold Harp!");
            strcpy_s(log2,"");
        } break; 
    case NECKLACE:
        {
            strcpy_s(log1,"There Is An Inscription On The Back!");
            strcpy_s(log2,"\"Protection From Traps!\"");
        } break; 
    case BOOK:
        {
            if ((player.wear & EYE_GLASSES)==0)
            {
                strcpy_s(log1,"The Words Are Too Blurry");
                strcpy_s(log2,"To Read");
            }
            else
            {
                strcpy_s(log1,"Book Title: \"The Gate\"");
                strcpy_s(log2,"   --Wave Scepter--   ");
            }
        } break; 
    case MAGIC_WAND:
        {
            strcpy_s(log1,"A Magical Silver Wand!");
            strcpy_s(log2,"");
        } break; 
    case SWORD:
        {
            strcpy_s(log1,"A Solid Steel Sword!");
            strcpy_s(log2,"");
        } break; 
    case KEY:
        {
            strcpy_s(log1,"A Rusty Looking Key!");
            strcpy_s(log2,"");
        } break; 
    case EYE_GLASSES:
        {
            strcpy_s(log1,"A Pair of Bifocals!");
            strcpy_s(log2,"");
        } break; 
    case CRYSTAL_BALL:
        {
            strcpy_s(log1,"You See A Man In A Winding");
            strcpy_s(log2,"Passage, Waving A Wand!");
        } break; 
    case HELMET:
        {
            strcpy_s(log1,"A Solid Looking Helmet!");
            strcpy_s(log2,"");
        } break;
    case WINE_FLASK:
        {
            strcpy_s(log1,"A Magical Wine Flask!");
            strcpy_s(log2,"");
        } break; 
    case HOLY_CROSS:
        {
            strcpy_s(log1,"A Gold Cross With Four");
            strcpy_s(log2,"Gems Set In The Points");
        } break; 
    case DIAMOND:
        {
            strcpy_s(log1,"A Flawless Perfect Cut Diamond!");
            strcpy_s(log2,"");
        } break; 
    case SILVER_BARS:
        {
            strcpy_s(log1,"Two Solid Silver Bars!");
            strcpy_s(log2,"");
        } break; 
    case RUBYS:
        {
            strcpy_s(log1,"A Large Beautiful Ruby!");
            strcpy_s(log2,"");
        } break; 
    case JADE_FIGURINE:
        {
            strcpy_s(log1,"A Solid Jade Figurine");
            strcpy_s(log2,"With Sapphires For Eyes!");
        } break; 
    case SCEPTER:
        {
            strcpy_s(log1,"A Firey Ruby Sits Atop");
            strcpy_s(log2,"This Powerful Scepter");
        } break; 
    case HOURGLASS:
        {
            strcpy_s(log1,"A Gold HourGlass With");
            strcpy_s(log2,"Diamond Sand!");
        } break; 
    case LARGE_GEM:
        {
            strcpy_s(log1,"A Very Large Priceless Gem!");
            strcpy_s(log2,"");
        } break; 
    case GOLDBAR:
        {
            strcpy_s(log1,"It Is A Solid Gold Bar!");
            strcpy_s(log2,"");
        } break; 
    case FANCY_GOBLET:
        {
            strcpy_s(log1,"It Is Made Out of Gold!");
            strcpy_s(log2,"");
        } break; 
    case CROWN:
        {
            strcpy_s(log1,"It Is Made Out Of Gold");
            strcpy_s(log2,"With Rubys Attached!");
        } break; 
    }
}

/*
 * runs the select item function and sends it to lookItem
 */
void look_item()
{
    int t=select_item("Look Item");
    if (t>=0)
    {
        lookItem(t);        
    }
}

/*
 * Shows the instructions on the screen
 */
void do_intro()
{
    BITMAP *temp;
    char text[14][40]={
        "You are trapped in a deserted Castle   ",
        "and you must escape. It is rumored that",
        "the  castle is  full of treasures.  Can",
        "you find them all?                     ",
        "                                       ",
        "Use the cursor keypad to move your man ",
        "around the rooms.  To pick up a visible",
        "item,  just run into it.  To pick up an",
        "item not displayed  on  the screen, use",
        "the Search key. To  attack  monsters",
        "just  run  into  them,  but only if you",
        "have a weapon. There are 80+ rooms & 13",
        "treasures. To  see the High Score, look",
        "at the wall in the courtyard.          "};
    int w,h;

    w=0;
    
    for (int i=0; i<14; i++)
    {
        if (w<text_length(myfont,text[i]))
            w=text_length(myfont,text[i]);
    }

    w+=8;
    h=14*TILESIZE+40;

    temp=create_bitmap(w,h);
    drawWindow(temp,w,h,0);

    textout_centre(temp,myfont,"Castle Adventure",w/2,0,makecol(255,255,255));

    for (int i=0; i<14; i++)
    {
        textout(temp,myfont,text[i],8,24+i*TILESIZE,makecol(255,255,255));
    }

    acquire_screen();
    draw_sprite(screen,temp,(SCREEN_W-w)/2,(SCREEN_H-h)/2);
    release_screen();

    while (!key[KEY_ENTER] && !key[KEY_ENTER_PAD] && !key[KEY_ESC] && !key[KEY_SPACE])
    {
    }

    destroy_bitmap(temp);
}

/*
 * When the game is over, compute the score
 */
int computeScore()
{
    int s=0;
    for (int i=0; i<MAX_MONSTER; i++)
    {
        if (monster[i].state==DEAD)
        {
            s+=monster[i].mhp;
        }
    }
    for (int i=9; i<MAX_ITEM; i++)
    {
        if ((player.item & item[i].type)!=0 || item[i].room==1)
            s+=50;
    }

    s+=player.bt*3;

    if (s>hs.score)
    {
        hs.score=s;
        strcpy_s(hs.name,player.name);
    }

    return s;
}

/*
 * game is over show final stats
 */
void do_end(int p)
{
    int mtype[8]={MON0000,MON1000,MON2000,MON3000,MON4000,MON5000,MON6000,MON7000};
    BITMAP *temp;
    char text[8][40]={"","",
                      "You have collected these Treasues",
                      "",
                      "You have killed these Monsters",
                      "",
                      "Your Score:        ",
                      "(1550 is perfect)"};
    
    sprintf_s(text[6],"Your Score: %d",computeScore());
    if (p==0) strcpy_s(text[0],"You Have Escaped the Castle!");
    if (p==1) strcpy_s(text[0],"You Died!");
    if (p==2) strcpy_s(text[0],"You Are A Cowered!");

    int w,h,k,c;

    w=416;
    
    for (int i=0; i<8; i++)
    {
        if (w<text_length(myfont,text[i]))
            w=text_length(myfont,text[i]);
    }

    w+=16;
    h=8*TILESIZE+40;

    temp=create_bitmap(w,h);
    drawWindow(temp,w,h,0);

    textout_centre(temp,myfont,"Game Over",w/2,0,makecol(255,255,255));

    for (int i=0; i<8; i++)
    {
        textout_centre(temp,myfont,text[i],w/2,24+i*TILESIZE,makecol(255,255,255));
    }

    k=0;
    for (int i=9; i<MAX_ITEM; i++)
    {
        if ((player.item & item[i].type)!=0 || item[i].room==1)
        {
            k++;
        }
    }

    if (k==0)
    {
        textout_centre(temp,myfont,"None",w/2,120,makecol(0,255,0));
    }
    else
    {
        c=(w-k*TILESIZE)/2;
        k=0;
        for (int i=9; i<MAX_ITEM; i++)
        {
            if ((player.item & item[i].type)!=0 || item[i].room==1)
            {
                draw_sprite(temp,(BITMAP*)datafile[ITEM000+item[i].gfx].dat,c+k*TILESIZE,120);
                k++;
            }
        }
    }

    k=0;
    for (int i=0; i<MAX_MONSTER-2; i++)
    {
        if (monster[i].state==DEAD)
        {
            k++;
        }
    }

    if (k==0)
    {
        textout_centre(temp,myfont,"None",w/2,184,makecol(255,0,0));
    }
    else
    {
        c=(w-k*TILESIZE)/2;
        k=0;
        for (int i=0; i<MAX_MONSTER-2; i++)
        {
            if (monster[i].state==DEAD)
            {
                draw_sprite(temp,(BITMAP*)datafile[mtype[monster[i].gfx]+4].dat,c+k*TILESIZE,184);
                k++;
            }
        }
    }

    acquire_screen();
    draw_sprite(screen,temp,(SCREEN_W-w)/2,(SCREEN_H-h)/2);
    release_screen();

    while (!key[KEY_ENTER] && !key[KEY_ENTER_PAD] && !key[KEY_ESC] && !key[KEY_SPACE])
    {
    }

    destroy_bitmap(temp);
}

/*
 * shows the current items you have
 */
void do_inventory()
{
    int w,h,k;
    BITMAP *temp;

    k=0;

    if (player.itemcount==0)
    {
        w=200;
        h=66;
    }
    else
    {
        w=200;
        h=40+34*player.itemcount;
    }

    temp=create_bitmap(w,h);
    drawWindow(temp,w,h,0);

    textout_centre(temp,myfont,"Inventory",w/2,0,makecol(255,255,255));

    if (player.itemcount==0)
    {
        textout(temp,myfont,"Nothing",8,24,makecol(255,255,255));
    }
    else
    {
        for (int i=0; i<MAX_ITEM; i++)
        {
            if ((player.item & item[i].type)!=0)
            {
           	    masked_blit((BITMAP*)datafile[ITEM000+item[i].gfx].dat,temp,0,0,8,24+k*TILESIZE,TILESIZE,TILESIZE);
                textout(temp,myfont,item[i].name,40,24+k*TILESIZE,makecol(255,255,255));
                k++;
            }
        }
    }

    acquire_screen();
    draw_sprite(screen,temp,(SCREEN_W-w)/2,(SCREEN_H-h)/2);
    release_screen();

    while (!key[KEY_ENTER] && !key[KEY_ENTER_PAD] && !key[KEY_ESC] && !key[KEY_SPACE])
    {
    }


    destroy_bitmap(temp);
}

/*
 * draws the two log strings on the bottom of the scren
 */
void drawLog(BITMAP *bmp)
{
	UNREFERENCED_PARAMETER(bmp);
    textout(buffer,myfont,log1,8,708,makecol(255,255,255));
    textout(buffer,myfont,log2,8,728,makecol(255,255,255)); 
}

/*
 * Draw the Function Key descriptions
 */
void drawKeys(BITMAP *bmp)
{
    int h=text_height(myfont);
    char k[10][40]={"Inventory","Look Room","Search","Drop Item","Use Item",
        "Look Item","Wear Item","Load Game","Save Game","Quit"};
    for (int i=0; i<5; i++)
    {
        textprintf(bmp,myfont,412,598+i*(h-6),makecol(255,255,255),"F%d %s",i+1,k[i]);
        textprintf(bmp,myfont,620,598+i*(h-6),makecol(255,255,255),"F%d %s",i+6,k[i+5]);
    }
}


// draw the buffer to the screen
void drawScreen(BITMAP *bmp)
{
    acquire_screen();
    blit(bmp,screen,0,0,0,0,SCREEN_W,SCREEN_H);
    release_screen();
}

/* 
 * draw everything
 */
void drawAll()
{
    clear_to_color(buffer,makecol(0,0,0));
    drawRoom(buffer);
    drawMonster(buffer);
    drawPlayer(buffer);
    drawItem(buffer);
    drawLog(buffer);
    drawKeys(buffer);
    if (key[KEY_T])
    textprintf(buffer,font,0,0,makecol(255,255,255),"Room %d, (%d,%d)",player.rm,player.x,player.y);
    drawWindow(buffer,SCREEN_W,SCREEN_H,1);
    drawScreen(buffer);
}

/*
 * change the frame of the character
 * for the direction given
 */
void inc_frame(int d)
{
	UNREFERENCED_PARAMETER(d);
    player.frame++;
	if (player.dir==UP)
	{	
                if (player.frame>1) player.frame=0;
	}
	if (player.dir==DOWN)
	{
                if (player.frame<4 || player.frame>5) player.frame=4;
	}
	if (player.dir==LEFT)
	{
                if (player.frame<6 || player.frame>7) player.frame=6;
	}
	if (player.dir==RIGHT)
	{
                if (player.frame<2 || player.frame>3) player.frame=2;
	}
}

/*
 * check if the player hits a wall
 */
bool hit_wall(int q,int x, int y)
{
	int x1,y1;
    char p;
        for (int i=0; i<TILESIZE; i++)
	{
                for (int j=0; j<TILESIZE; j++)
		{
			x1=(x+i)/TILESIZE;
			y1=(y+j)/TILESIZE;
            p=room[player.rm-1].data[y1][x1];
            if (p!=' ' && p!='*' && p!='#')
            {
                if (q==1)
                {
                    if (p=='f')
                    {
                        if (player.rm==68)
                        {
                            if ((player.item & KEY)!=0)
                            {
                                room[67].data[15][8]=' ';
                                room[67].data[15][9]=' ';  
                                return false;
                            }
                            else
                            {
                                strcpy_s(log1,"The Door Is Locked!");
                                strcpy_s(log2,"");
                            }                            
                        }
                        if (player.rm==1)
                        {
                            strcpy_s(log1,"The Gate Is Magically Sealed!");
                            strcpy_s(log2,"");
                        }
                    }
                    
                    if (p=='m') 
                    {
                        stair=1;
                        return true;
                    }
                    if (p=='n') 
                    {
                        stair=2;
                        return true;
                    }
                    if (p>='A' && p<='W')
                    {
                        if (player.itemcount<MAX_ITEM_CARRY)
                        {
                            player.item|=item[p-'A'].type;
                            room[player.rm-1].data[y1][x1]=' ';
                            player.itemcount++;
                            sprintf_s(log1,"You Found a %s!",item[p-'A'].name);
                            strcpy_s(log2,"");
                            return false;
                        }
                        else
                        {
                            strcpy_s(log2,"But You Can't Carry Any More!");
                            return true;
                        }
                    }
                }
                return true;
            }
		}
	}

	return false;
}

/*
 * check if where the player is if he hits a monster
 * also used to see if the monster hit the player
 */
bool hit_monster(int x, int y)
{
        for (int k=0; k<MAX_MONSTER; k++)
    {
        if (monster[k].room==player.rm && monster[k].state==ACTIVE)
        {
            if (ABS((x+16)-(monster[k].x+16))<TILESIZE &&
                ABS((y+16)-(monster[k].y+16))<TILESIZE)
            {
                if (monster[k].type==VAMPIRE || monster[k].type==FAIRY1 || monster[k].type==FAIRY2)
                {
                    sprintf_s(log1,"The Is A %s Blocking Your Path,",monster[k].name);
                    strcpy_s(log2,"But It Can't Be Hurt!");
                }
                return true;
            }
        }
    }

	return false;
}

/*
 * move the monster in the direction of the player
 */
void moveMonsters()
{
    int a,h=-1;
    for (int i=0; i<MAX_MONSTER; i++)
    {
        if (monster[i].room==player.rm && monster[i].state!=DEAD)
        {   
            a=128-atan2(monster[i].x-player.x,monster[i].y-player.y);
            if (i!=8) monster[i].frame++;
            monster[i].dir=RIGHT;

            if (a>=224 || a<32) monster[i].dir=DOWN;
            if (a>=32 && a<96) monster[i].dir=LEFT;
            if (a>=96 && a<160) monster[i].dir=UP;
            if (a>=160 && a<224) monster[i].dir=RIGHT;

            if (i<=7)
            { 
                if (monster[i].y>player.y)
                {
                    monster[i].y-=8;
                    if (hit_monster(player.x,player.y))
                    {
                        h=i;
                        monster[i].y+=8;
                    }
                    if (hit_wall(0,monster[i].x,monster[i].y)) monster[i].y+=8;
                }
                if (monster[i].y<player.y)
                {
                    monster[i].y+=8;
                    if (hit_monster(player.x,player.y))
                    {
                        h=i;
                        monster[i].y-=8;
                    }
                    if (hit_wall(0,monster[i].x,monster[i].y)) monster[i].y-=8;
                }
                if (monster[i].x>player.x)
                {
                    monster[i].x-=8;
                    if (hit_monster(player.x,player.y))
                    {
                        h=i;
                        monster[i].x+=8;
                    }
                    if (hit_wall(0,monster[i].x,monster[i].y)) monster[i].x+=8;
                }
                if (monster[i].x<player.x)
                {
                    monster[i].x+=8;
                    if (hit_monster(player.x,player.y))
                    {
                        h=i;
                        monster[i].x-=8;
                    }
                    if (hit_wall(0,monster[i].x,monster[i].y)) monster[i].x-=8;
                }
            }


	        if (monster[i].dir==UP)
	        {	
                if (monster[i].frame>1) monster[i].frame=0;
	        }
	        if (monster[i].dir==DOWN)
	        {
                if (monster[i].frame<4 || monster[i].frame>5) monster[i].frame=4;
	        }
	        if (monster[i].dir==LEFT)
	        {
                if (monster[i].frame<6 || monster[i].frame>7) monster[i].frame=6;
	        }
	        if (monster[i].dir==RIGHT)
	        {
                if (monster[i].frame<2 || monster[i].frame>3) monster[i].frame=2;
	        }
        }
    }

    if (h>=0)
    {
        if (Random(10)<4)
        {
            player.hp-=monster[h].hit;
            if (player.hp<1) gameover=true;
            sprintf_s(log2,"The %s %s Hit You!",monster[h].adj,monster[h].name);
        }
        else
        {
            sprintf_s(log2,"The %s %s Missed You!",monster[h].adj,monster[h].name);
        }
    }
}

/*
 * move in the direction supplied
 */
void move(int d)
{
    int h=-1;
	player.dir=d;
	inc_frame(player.dir);
	if (player.dir==LEFT)
	{
		player.x-=16;
		if (hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y))
		{
            if (hit_monster(player.x,player.y))
            {
                for (int i=0; i<8; i++)
                {
                    if (monster[i].room==player.rm)
                        h=i;
                }
            }
			player.x+=16;		
			do
			{
				player.x--;
			} while (!(hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y)));		
			player.x++;
		}
	}
	if (player.dir==RIGHT)
	{
		player.x+=16;
		if (hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y))
		{
            if (hit_monster(player.x,player.y))
            {
                for (int i=0; i<8; i++)
                {
                    if (monster[i].room==player.rm)
                        h=i;
                }
            }
			player.x-=16;		
			do
			{
				player.x++;
			} while (!(hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y)));		
			player.x--;
		}
	}
	if (player.dir==UP)
	{
		player.y-=16;
		if (hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y))
		{
            if (hit_monster(player.x,player.y))
            {
                for (int i=0; i<8; i++)
                {
                    if (monster[i].room==player.rm)
                        h=i;
                }
            }
			player.y+=16;		
			do
			{
				player.y--;
			} while (!(hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y)));		
			player.y++;
		}
	}
	if (player.dir==DOWN)
	{
		player.y+=16;
		if (hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y))
		{
            if (hit_monster(player.x,player.y))
            {
                for (int i=0; i<8; i++)
                {
                    if (monster[i].room==player.rm)
                        h=i;
                }
            }
			player.y-=16;		
			do
			{
				player.y++;
			} while (!(hit_wall(1,player.x,player.y) || hit_monster(player.x,player.y)));		
			player.y--;
		}
	}
    if (h>=0 && h<8)
    {
        if (Random(10)<4 && (player.item & SWORD)!=0)
        {
            monster[h].hp-=10+Random(10);
            sprintf_s(log1,"You Hit The %s %s!",monster[h].adj,monster[h].name);
            if (monster[h].hp<1)
            {
                sprintf_s(log1,"You Killed The %s %s!",monster[h].adj,monster[h].name);
                monster[h].state=DEAD;
                strcpy_s(monster[h].adj,"Dead");
            }
            strcpy_s(log2,"");
        }
        else
        {
            sprintf_s(log1,"You Missed The %s %s!",monster[h].adj,monster[h].name);
            strcpy_s(log2,"");
        }
    }
}

/*
 * one of the rooms has a trap, Do a little animation of the room
 * filling with water.
 */
void do_flood()
{
    char s1[40],s2[40];

    strcpy_s(s1,room[81].data[8]);
    strcpy_s(s2,room[81].data[9]);

    s1[1]='c';s1[12]='c';
    s2[1]='c';s2[12]='c';

    strcpy_s(room[81].data[8],s1);
    strcpy_s(room[81].data[9],s2);
    for (int i=2; i<=11; i++)
    {
        s1[i]='t';
        s2[i]='t';

        strcpy_s(room[81].data[8],s1);
        strcpy_s(room[81].data[9],s2);
        drawAll();
    }
}

/*
 * two rooms have special requirments, one is a trap the other
 * needs light from the lamp
 */
void check_special()
{
    if (player.rm==82 && player.x>96 && player.x<320 && (player.wear & NECKLACE)==0)
    {
        strcpy_s(log1,"Oh No! A Booby Trap!");
        drawAll();
        do_flood();
        strcpy_s(log2,"You Drowned!");
        
        drawAll();
        rest(1000);
        gameover=true;
    }
    if (player.rm==68 && (player.item & LAMP)==0)
    {
        change_room(room[player.rm-1].exits[4]);
        strcpy_s(log1,"It's Too Dark To See Down There!");
        strcpy_s(log2,"");
    }
}

/*
 * when you leave the room, we need to save the monsters current
 * coordinates and make him inactive
 */
void save_monster(int r)
{
    for (int i=0; i<MAX_MONSTER; i++)
    {
        if (monster[i].room==r)
        {
            monster[i].y=monster[i].y/TILESIZE;
            monster[i].x=monster[i].x/TILESIZE;
            if (monster[i].state!=DEAD) monster[i].state=INACTIVE;
        }
    }
}

/*
 * we need to activate the monster in the room
 */
void load_monster(int r)
{
    for (int i=0; i<MAX_MONSTER; i++)
    {
        if (monster[i].room==r)
        {
            monster[i].y=monster[i].y*TILESIZE;
            monster[i].x=monster[i].x*TILESIZE;
            if (monster[i].state!=DEAD) monster[i].state=ACTIVE;
        }
    }
}

/*
 * we need to do any changes when we change room
 * like activating/deactivating monsters
 * clear log. If you haven't been to this room then increase
 * room.beento
 */
void change_room(int r)
{
    strcpy_s(log1,"");
    strcpy_s(log2,"");
    save_monster(player.rm);
    player.rm=r;
    load_monster(player.rm);
    if (room[player.rm-1].beento==0)
    {
        room[player.rm-1].beento=1;
        player.bt++;
    }
}

/*
 * do a search of the area, like desks and shelves. The player must
 * be close enough to the hidden object to find it
 */
void do_search()
{
    int p=false;
    for (int i=0; i<MAX_ITEM; i++)
    {
        if (item[i].room>84)
        {
            if (room[player.rm-1].data[(player.y)/TILESIZE][(player.x)/TILESIZE]=='#' ||
                room[player.rm-1].data[(player.y+TILESIZE-1)/TILESIZE][(player.x)/TILESIZE]=='#' ||
                room[player.rm-1].data[(player.y)/TILESIZE][(player.x+TILESIZE-1)/TILESIZE]=='#' ||
                room[player.rm-1].data[(player.y+TILESIZE-1)/TILESIZE][(player.x+TILESIZE-1)/TILESIZE]=='#')
            {
                if (item[i].room-100==player.rm && (player.item & item[i].type)==0)
                {
                    sprintf_s(log1," You found a %s!",item[i].name);
                    if (player.itemcount==MAX_ITEM_CARRY)
                    {
                        strcpy_s(log2,"But You Can't Carry Any More!");                        
                    }
                    else
                    {
                        player.item|=item[i].type;
                        player.itemcount++;
                    }
                    p=true;
                }
            }
        }
        else
        {
           
        }
    }

    if (!p)
    {
        strcpy_s(log1,"You Didn't Find Anything!");
        strcpy_s(log2,"");
    }
    while (key[KEY_F3]) {}
}

/*
 * look around the room. 
 */
void do_look()
{
    int p=false;
    char msg1[80],msg2[80];
    switch (player.rm)
    {
    case 1:
        {
            strcpy_s(log1,"There Is Something Scratched In the Wall!");
            sprintf_s(log2,"High Score: %s %d",hs.name,hs.score);
            p=true;
        } break;
    case 19:
        {
            strcpy_s(log1,"You See A Statue Of The King!");
            if (item[2].room>83 && (player.item & NECKLACE)==0)
                strcpy_s(log2,"The Statue is Wearing a Necklace!");
            else
                strcpy_s(log2,"");
            p=true;
        } break;
    case 33:
        {
            if (room[32].data[(player.y)/TILESIZE][(player.x)/TILESIZE]=='#' ||
                room[32].data[(player.y+TILESIZE-1)/TILESIZE][(player.x)/TILESIZE]=='#' ||
                room[32].data[(player.y)/TILESIZE][(player.x+TILESIZE-1)/TILESIZE]=='#' ||
                room[32].data[(player.y+TILESIZE-1)/TILESIZE][(player.x+TILESIZE-1)/TILESIZE]=='#')
            {
                strcpy_s(log1,"You Look Out Over The Garden!");
                if (item[18].room>83 && (player.item & LARGE_GEM)==0)
                    strcpy_s(log2,"There Is Something in the Fountain!");
                else
                    strcpy_s(log2,"");
                p=true;
            }
        } break;
    case 58:
        {
            strcpy_s(log1,"The Shelves Are Empty, But There");
            strcpy_s(log2,"Somthing On That Middle Shelf");
            p=true;
        } break;
    case 74:
        {
            strcpy_s(log1,"You See Something Written On The Walls!");
            strcpy_s(log2,"\"Kevin Bales Was Here!\"");
            p=true;
        } break;
    case 79:
        {
            strcpy_s(msg1,"There Are Blood Stains");
            strcpy_s(msg2,"On The Table! Yuch!");
            p=true;
        } break;
    }

    if (!p)
    {
        strcpy_s(log1,"You Don't See Anything Special!");
        strcpy_s(log2,"");
    }
    while (key[KEY_F2]) {}
}

/*
 * main loop
 */
int main()
{
    bool done=false;

    if (initAll()<0) return -1;

    do_intro();

    while (!done)
    {
        drawAll();

		LastSystemTime = GetTickCount();

        if (key[KEY_LEFT])
        {
            if (player.x<=0 && room[player.rm-1].exits[3]!=0)
            {
                change_room(room[player.rm-1].exits[3]);
                player.x=736;
            }
			move(LEFT);
            if (stair!=0)
            {
                while (key[KEY_LEFT]) {}
                player.dir=RIGHT;
                change_room(room[player.rm-1].exits[3+stair]);
                stair=0;
            }
        }
        if (key[KEY_RIGHT])
        {
            if (player.x>=736 && room[player.rm-1].exits[2]!=0)
            {
                change_room(room[player.rm-1].exits[2]);
                player.x=0;
            }
			move(RIGHT);
            if (stair!=0)
            {
                while (key[KEY_RIGHT]) {}
                player.dir=LEFT;
                change_room(room[player.rm-1].exits[3+stair]);
                stair=0;
            }
        }
        if (key[KEY_UP])
        {
            if (player.y<=0 && room[player.rm-1].exits[0]!=0)
            {
                change_room(room[player.rm-1].exits[0]);
                player.y=544;
            }
            move(UP);
            if (stair!=0)
            {
                while (key[KEY_UP]) {}
                player.dir=DOWN;
                change_room(room[player.rm-1].exits[3+stair]);
                stair=0;
            }
        }
        if (key[KEY_DOWN])
        {
            if (player.y>=544 && room[player.rm-1].exits[1]!=0)
            {
                change_room(room[player.rm-1].exits[1]);
                player.y=0;
            }
            move(DOWN);
            if (stair!=0)
            {
                while (key[KEY_DOWN]) {}
                player.dir=UP;
                change_room(room[player.rm-1].exits[3+stair]);
                stair=0;
            }
		}

        check_special();

        if (count==1)
        {
            count=0;
            moveMonsters();
        }


        if (key[KEY_F1]) do_inventory();
        if (key[KEY_F2]) do_look();
        if (key[KEY_F3]) do_search();
        if (key[KEY_F4]) do_drop();
        if (key[KEY_F5]) use_item();
        if (key[KEY_F6]) look_item();
        if (key[KEY_F7]) wear_item();
        if (key[KEY_F8]) load_game();
        if (key[KEY_F9]) save_game();

        if (player.rm==84) gameover=true;

        if (key[KEY_F10] || key[KEY_ESC]) 
        {
            if (areSure("Are You Sure That You","Want To Quit? <Y/N>"))
                done=true;
        }
      
        if (gameover) done=true;
		
		CurrentDelay = GetTickCount() - LastSystemTime;
		if (CurrentDelay < SLEEPTIME) Sleep(SLEEPTIME - CurrentDelay);
	}

    if (gameover)
    {
        if (player.hp<1)
        {
            do_end(1); // died
        }
        else
        {
            do_end(0);  // escaped
        }
    }
    else
    {
        do_end(2); // quit
    }

    deleteAll();
    return 1;
}

END_OF_MAIN();
