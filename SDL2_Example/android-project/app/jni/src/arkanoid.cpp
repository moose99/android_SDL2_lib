//
// Created by moose-home on 7/22/2018.
//

#include <iostream>
#include <memory>
#include <typeinfo>
#include <map>
#include<string>
#include <ostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include "SDL.h"
#include "SDL_ttf.h"

const Uint8* gCurrentKeyStates = nullptr;

// global vars, TODO - move these into the game
SDL_Rect gScreenRect = { 0, 0, 800, 600 };
SDL_Point gTouchLocation = { gScreenRect.w / 2, gScreenRect.h / 2 };
bool gTouchTap = false;

/**
* Draw an SDL_Texture to an SDL_Renderer at some destination rect
* taking a clip of the texture if desired
* @param tex The source texture we want to draw
* @param ren The renderer we want to draw to
* @param dst The destination rectangle to render the texture to
* @param clip The sub-section of the texture to draw (clipping rect)
*        default of nullptr draws the entire texture
*/
void renderTexture( SDL_Texture *tex, SDL_Renderer *ren, SDL_Rect dst,
                    SDL_Rect *clip = nullptr )
{
    SDL_RenderCopy( ren, tex, clip, &dst );
}

/**
* Draw an SDL_Texture to an SDL_Renderer at position x, y, preserving
* the texture's width and height and taking a clip of the texture if desired
* If a clip is passed, the clip's width and height will be used instead of
*    the texture's
* @param tex The source texture we want to draw
* @param ren The renderer we want to draw to
* @param x The x coordinate to draw to
* @param y The y coordinate to draw to
* @param clip The sub-section of the texture to draw (clipping rect)
*        default of nullptr draws the entire texture
*/
void renderTexture( SDL_Texture *tex, SDL_Renderer *ren, int x, int y, SDL_Rect *clip = nullptr )
{
    SDL_Rect dst;
    dst.x = x;
    dst.y = y;
    if (clip != nullptr)
    {
        dst.w = clip->w;
        dst.h = clip->h;
    }
    else
    {
        SDL_QueryTexture( tex, NULL, NULL, &dst.w, &dst.h );
    }
    renderTexture( tex, ren, dst, clip );
}


/**
* Log an SDL error with some error message to the output stream of our choice
* @param os The output stream to write the message to
* @param msg The error message to write, format will be msg error: SDL_GetError()
*/
void logSDLError( std::ostream &os, const std::string &msg )
{
    os << msg << " error: " << SDL_GetError() << std::endl;
    SDL_Log("error: %s, %s\n", msg.c_str(), SDL_GetError());
}

/**
* Render the message we want to display to a texture for drawing
* @param message The message we want to display
* @param fontFile The font we want to use to render the text
* @param color The color we want the text to be
* @param fontSize The size we want the font to be
* @param renderer The renderer to load the texture in
* @return An SDL_Texture containing the rendered message, or nullptr if something went wrong
*/
SDL_Texture* createText( const std::string &message, TTF_Font *font,
                         SDL_Color color, SDL_Renderer *renderer )
{
    //We need to first render to a surface as that's what TTF_RenderText
    //returns, then load that surface into a texture
    SDL_Surface *surf = TTF_RenderText_Blended( font, message.c_str(), color );
    if (surf == nullptr)
    {
        TTF_CloseFont( font );
        logSDLError( std::cout, "TTF_RenderText" );
        return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface( renderer, surf );
    if (texture == nullptr)
    {
        logSDLError( std::cout, "CreateTexture" );
    }
    //Clean up the surface and font
    SDL_FreeSurface( surf );
    return texture;
}

void fill_circle( SDL_Renderer *renderer, int cx, int cy, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
    // Note that there is more to altering the bitrate of this
    // method than just changing this value.  See how pixels are
    // altered at the following web page for tips:
    //   http://www.libsdl.org/intro.en/usingvideo.html
    static const int BPP = 4;

    for (double dy = 1; dy <= radius; dy += 1.0)
    {
        // This loop is unrolled a bit, only iterating through half of the
        // height of the circle.  The result is used to draw a scan line and
        // its mirror image below it.

        // The following formula has been simplified from our original.  We
        // are using half of the width of the circle because we are provided
        // with a center and we need left/right coordinates.

        double dx = floor( sqrt( (2.0 * radius * dy) - (dy * dy) ) );
        int x = cx - dx;
        SDL_SetRenderDrawColor( renderer, r, g, b, a );
        SDL_RenderDrawLine( renderer, cx - dx, cy + dy - radius, cx + dx, cy + dy - radius );
        SDL_RenderDrawLine( renderer, cx - dx, cy - dy + radius, cx + dx, cy - dy + radius );
    }
}


class Entity
{
public:
    bool destroyed{ false };

    virtual ~Entity() {}
    virtual void update() {}
    virtual void draw( SDL_Renderer *renderer ) {}
};

class Manager
{
private:
    std::vector<std::unique_ptr<Entity>> entities;
    std::map<std::size_t, std::vector<Entity*>> groupedEntities;

public:
    template <typename T, typename... TArgs>
    T& create( TArgs&&... mArgs )
    {
        static_assert(std::is_base_of<Entity, T>::value,
                      "`T` must be derived from `Entity`");

        auto uPtr( std::make_unique<T>( std::forward<TArgs>( mArgs )... ) );
        auto ptr( uPtr.get() );
        groupedEntities[typeid(T).hash_code()].emplace_back( ptr );
        entities.emplace_back( std::move( uPtr ) );

        return *ptr;
    }

    void refresh()
    {
        for (auto& pair : groupedEntities)
        {
            auto& vector( pair.second );

            vector.erase( std::remove_if( std::begin( vector ), std::end( vector ),
                                          []( auto mPtr )
                                          {
                                              return mPtr->destroyed;
                                          } ),
                          std::end( vector ) );
        }

        entities.erase( std::remove_if( std::begin( entities ), std::end( entities ),
                                        []( const auto& mUPtr )
                                        {
                                            return mUPtr->destroyed;
                                        } ),
                        std::end( entities ) );
    }

    void clear()
    {
        groupedEntities.clear();
        entities.clear();
    }

    template <typename T>
    auto& getAll()
    {
        return groupedEntities[typeid(T).hash_code()];
    }

    template <typename T, typename TFunc>
    void forEach( const TFunc& mFunc )
    {
        for (auto ptr : getAll<T>()) mFunc( *reinterpret_cast<T*>(ptr) );
    }

    void update()
    {
        for (auto& e : entities) e->update();
    }
    void draw( SDL_Renderer *renderer )
    {
        for (auto& e : entities) e->draw( renderer );
    }
};

struct Vector2f
{
    float x, y;
};

struct Shape
{
    float x, y;
    float originX, originY;
    SDL_Color fillColor;

    void move( const Vector2f &vel ) { x += vel.x; y += vel.y; }
    void setPosition( float mX, float mY ) { x = mX; y = mY; }
    void setFillColor( const SDL_Color &defColor ) { fillColor = defColor; }
    void setOrigin( float oX, float oY ) { originX = oX; originY = oY; }
};

struct Rectangle : public Shape
{
    float w, h;

    void setSize( float width, float height ) { w = width; h = height; }
    float width() const noexcept { return w; }
    float height() const noexcept { return h; }
    float left() const noexcept { return x - width() / 2.f; }
    float right() const noexcept { return x + width() / 2.f; }
    float top() const noexcept { return y - height() / 2.f; }
    float bottom() const noexcept { return y + height() / 2.f; }

    void drawShape( SDL_Renderer *renderer )
    {
        SDL_Rect rect = { (int)(x-originX), (int)(y-originY), (int)w, (int)h };	// TODO - round
        SDL_SetRenderDrawColor( renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a );
        SDL_RenderFillRect( renderer, &rect );
    }
};

// shape info
struct Circle : public Shape
{
    float radius;

    float left() const noexcept { return x - radius; }
    float right() const noexcept { return x + radius; }
    float top() const noexcept { return y - radius; }
    float bottom() const noexcept { return y + radius; }
    void setRadius( float r ) { radius = r; }
    void drawShape( SDL_Renderer *renderer )
    {	// TODO - round
        fill_circle( renderer, x-originX, y-originY, radius, fillColor.r, fillColor.g, fillColor.b, fillColor.a );
    }
};

class Ball : public Entity, public Circle
{
public:
    static const SDL_Color defColor;
    static constexpr float defRadius{ 10.f }, defVelocity{ 8.f };

    Vector2f velocity{ -defVelocity, -defVelocity };

    Ball( float mX, float mY )
    {
        setPosition( mX, mY );
        setRadius( defRadius );
        setFillColor( defColor );
        setOrigin( defRadius, defRadius );
    }

    void update() override
    {
        move( velocity );
        solveBoundCollisions();
    }

    void draw( SDL_Renderer *renderer ) override
    {
        drawShape( renderer );
    }

private:
    void solveBoundCollisions() noexcept
    {
        if (left() < 0)
            velocity.x = defVelocity;
        else if (right() > gScreenRect.w)
            velocity.x = -defVelocity;

        if (top() < 0)
            velocity.y = defVelocity;
        else if (bottom() > gScreenRect.h)
        {
            // If the ball leaves the window towards the bottom,
            // we destroy it.
            destroyed = true;
        }
    }
};

const SDL_Color Ball::defColor{ 255,0,0,255 };

class Paddle : public Entity, public Rectangle
{
public:
    static const SDL_Color defColor;
    static constexpr float defWidth{ 60.f }, defHeight{ 20.f };
    static constexpr float defVelocity{ 8.f };

    Vector2f velocity = { 0,0 };

    Paddle( float mX, float mY )
    {
        setPosition( mX, mY );
        setSize( defWidth, defHeight );
        setFillColor( defColor );
        setOrigin( defWidth / 2.f, defHeight / 2.f );
    }

    void update() override
    {
        processPlayerInput();
        move( velocity );
    }

    void draw( SDL_Renderer *renderer ) override
    {
        drawShape( renderer );
    }

private:
    void processPlayerInput()
    {
#if 0
        if ((gCurrentKeyStates[SDL_SCANCODE_LEFT]) && left() > 0)
            velocity.x = -defVelocity;
        else if ((gCurrentKeyStates[SDL_SCANCODE_RIGHT]) && right() < gScreenRect.w)
            velocity.x = defVelocity;
        else
            velocity.x = 0;
#endif
        x = gTouchLocation.x;
    }
};

const SDL_Color Paddle::defColor{ 255,0,0,255 };

class Brick : public Entity, public Rectangle
{
public:
    static const SDL_Color defColorHits1;
    static const SDL_Color defColorHits2;
    static const SDL_Color defColorHits3;
    static constexpr float defWidth{ 60.f }, defHeight{ 20.f };
    static constexpr float defVelocity{ 8.f };

    // Let's add a field for the required hits.
    int requiredHits{ 1 };

    Brick( float mX, float mY )
    {
        setPosition( mX, mY );
        setSize( defWidth, defHeight );
        setOrigin( defWidth / 2.f, defHeight / 2.f );
    }

    void update() override
    {
        // Let's alter the color of the brick depending on the
        // required hits.
        if (requiredHits == 1)
            setFillColor( defColorHits1 );
        else if (requiredHits == 2)
            setFillColor( defColorHits2 );
        else
            setFillColor( defColorHits3 );
    }
    void draw( SDL_Renderer *renderer ) override
    {
        drawShape( renderer );
    }
};

const SDL_Color Brick::defColorHits1{ 255, 255, 0, 80 };
const SDL_Color Brick::defColorHits2{ 255, 255, 0, 170 };
const SDL_Color Brick::defColorHits3{ 255, 255, 0, 255 };

template <typename T1, typename T2>
bool isIntersecting( const T1& mA, const T2& mB ) noexcept
{
    return mA.right() >= mB.left() && mA.left() <= mB.right() &&
           mA.bottom() >= mB.top() && mA.top() <= mB.bottom();
}

void solvePaddleBallCollision( const Paddle& mPaddle, Ball& mBall ) noexcept
{
    if (!isIntersecting( mPaddle, mBall )) return;

    mBall.velocity.y = -Ball::defVelocity;
    mBall.velocity.x =
            mBall.x < mPaddle.x ? -Ball::defVelocity : Ball::defVelocity;
}

void solveBrickBallCollision( Brick& mBrick, Ball& mBall ) noexcept
{
    if (!isIntersecting( mBrick, mBall )) return;

    // Instead of immediately destroying the brick upon collision,
    // we decrease and check its required hits first.
    --mBrick.requiredHits;
    if (mBrick.requiredHits <= 0) mBrick.destroyed = true;

    float overlapLeft{ mBall.right() - mBrick.left() };
    float overlapRight{ mBrick.right() - mBall.left() };
    float overlapTop{ mBall.bottom() - mBrick.top() };
    float overlapBottom{ mBrick.bottom() - mBall.top() };

    bool ballFromLeft( std::abs( overlapLeft ) < std::abs( overlapRight ) );
    bool ballFromTop( std::abs( overlapTop ) < std::abs( overlapBottom ) );

    float minOverlapX{ ballFromLeft ? overlapLeft : overlapRight };
    float minOverlapY{ ballFromTop ? overlapTop : overlapBottom };

    if (std::abs( minOverlapX ) < std::abs( minOverlapY ))
        mBall.velocity.x =
                ballFromLeft ? -Ball::defVelocity : Ball::defVelocity;
    else
        mBall.velocity.y = ballFromTop ? -Ball::defVelocity : Ball::defVelocity;
}


class Game
{
private:
    // There now are two additional game states: `GameOver`
    // and `Victory`.
    enum class State
    {
        Paused,
        GameOver,
        InProgress,
        Victory
    };

    static constexpr int brkCountX{ 11 }, brkCountY{ 4 };
    static constexpr int brkStartColumn{ 1 }, brkStartRow{ 2 };
    static constexpr float brkSpacing{ 3.f }, brkOffsetX{ 22.f };

    Manager manager;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    TTF_Font *font15 = nullptr;
    TTF_Font *font35 = nullptr;
    SDL_Texture *textState = nullptr;
    SDL_Texture *textLives = nullptr;

    State state{ State::GameOver };
    bool pausePressedLastFrame{ false };

    // Let's keep track of the remaning lives in the game class.
    int remainingLives{ 0 };

public:

    Game()
    {
    }

    // returns -1 on error
    int init()
    {
        // init SDL
        if (SDL_Init( SDL_INIT_EVERYTHING ) != 0)
        {
            logSDLError( std::cout, "SDL_Init" );
            return -1;
        }

        // init TTF system
        if (TTF_Init() != 0)
        {
            logSDLError( std::cout, "TTF_Init" );
            SDL_Quit();
            return -1;
        }

        // Create window
        window = SDL_CreateWindow( "ARKANOID", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   gScreenRect.w, gScreenRect.h, SDL_WINDOW_FULLSCREEN );
        if (window == nullptr)
        {
            logSDLError( std::cout, "CreateWindow" );
            SDL_Quit();
            return -1;
        }

        // create renderer
        renderer = SDL_CreateRenderer( window, -1,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
        if (renderer == nullptr)
        {
            logSDLError( std::cout, "CreateRenderer" );
            SDL_DestroyWindow( window );
            SDL_Quit();
            return -1;
        }
        if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0)
        {
            logSDLError(std::cout, "SetRenderDrawBlendMode");
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }
        SDL_RenderSetLogicalSize(renderer, gScreenRect.w, gScreenRect.h);

        //Open the font
        font15 = TTF_OpenFont( "calibri.ttf", 15 );
        font35 = TTF_OpenFont( "calibri.ttf", 35 );
        if (font15 == nullptr || font35 == nullptr)
        {
            logSDLError( std::cout, "TTF_OpenFont" );
            return -1;
        }

        SDL_Color white = { 255, 255, 255, 255 };
        textState = createText( "Paused", font35, white, renderer );
        textLives = createText( "Lives: 3", font15, white, renderer );
        if (textState == nullptr || textLives == nullptr)
        {
            SDL_DestroyRenderer( renderer );
            SDL_DestroyWindow( window );
            TTF_Quit();
            SDL_Quit();
            return -1;
        }

        return 0;	// ok
    }

    void restart()
    {
        // Let's remember to reset the remaining lives.
        remainingLives = 3;

        state = State::Paused;
        manager.clear();

        for (int iX{ 0 }; iX < brkCountX; ++iX)
            for (int iY{ 0 }; iY < brkCountY; ++iY)
            {
                float x{ (iX + brkStartColumn) * (Brick::defWidth + brkSpacing) };
                float y{ (iY + brkStartRow) * (Brick::defHeight + brkSpacing) };

                auto& brick( manager.create<Brick>( brkOffsetX + x, y ) );

                // Let's set the required hits for the bricks.
                brick.requiredHits = 1 + ((iX * iY) % 3);
            }

        manager.create<Ball>( gScreenRect.w / 2.f, gScreenRect.h / 2.f );
        manager.create<Paddle>( gScreenRect.w / 2.f, gScreenRect.h - 50.0f );
    }

    void run()
    {
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Event e;
        bool quit = false;
        while (!quit)
        {
            gTouchTap = false;
            while (SDL_PollEvent( &e ))
            {
                //If user closes the window, presses escape
                if (e.type == SDL_QUIT ||
                    (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                {
                    quit = true;
                }

                    //Touch down
                else if( e.type == SDL_FINGERDOWN )
                {
                    gTouchLocation.x = e.tfinger.x * gScreenRect.w;
                    gTouchLocation.y = e.tfinger.y * gScreenRect.h;
                    gTouchTap = true;
                }
                    //Touch motion
                else if( e.type == SDL_FINGERMOTION )
                {
                    gTouchLocation.x = e.tfinger.x * gScreenRect.w;
                    gTouchLocation.y = e.tfinger.y * gScreenRect.h;
                }
                    //Touch release
                else if( e.type == SDL_FINGERUP )
                {
                    gTouchLocation.x = e.tfinger.x * gScreenRect.w;
                    gTouchLocation.y = e.tfinger.y * gScreenRect.h;
                }
            }

            // key state
            gCurrentKeyStates = SDL_GetKeyboardState( NULL );

            /* Select the color for drawing. */
            SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
            //First clear the renderer with the draw color
            SDL_RenderClear( renderer );

            if (gCurrentKeyStates[SDL_SCANCODE_ESCAPE])
                break;

            if (gCurrentKeyStates[SDL_SCANCODE_P] || gTouchTap)
            {
                if (!pausePressedLastFrame)
                {
                    if (state == State::Paused)
                        state = State::InProgress;
#if 0
                    else if (state == State::InProgress)
                        state = State::Paused;
#endif
                }
                pausePressedLastFrame = true;
            }
            else
                pausePressedLastFrame = false;

            if (gCurrentKeyStates[SDL_SCANCODE_R] ||
                    (state == State::GameOver && gTouchTap) )
                restart();

            // If the game is not in progress, do not draw or update
            // game elements and display information to the player.
            if (state != State::InProgress)
            {
                std::string temp;
                if (state == State::Paused)
                    temp = "Paused";
                else if (state == State::GameOver)
                    temp = "Game over!";
                else if (state == State::Victory)
                    temp = "You won!";

                SDL_DestroyTexture(textState);
                textState = createText(temp, font35, white, renderer);
                renderTexture( textState, renderer, 10, 10 );
            }
            else
            {
                bool updateLives = false;
                // If there are no more balls on the screen, spawn a
                // new one and remove a life.
                if (manager.getAll<Ball>().empty())
                {
                    manager.create<Ball>( gScreenRect.w / 2.f, gScreenRect.h / 2.f );
                    updateLives = true;
                    --remainingLives;
                }

                // If there are no more bricks on the screen,
                // the player won!
                if (manager.getAll<Brick>().empty())
                    state = State::Victory;

                // If the player has no more remaining lives,
                // it's game over!
                if (remainingLives <= 0)
                    state = State::GameOver;

                manager.update();

                manager.forEach<Ball>( [this]( auto& mBall )
                                       {
                                           manager.forEach<Brick>( [&mBall]( auto& mBrick )
                                                                   {
                                                                       solveBrickBallCollision( mBrick, mBall );
                                                                   } );
                                           manager.forEach<Paddle>( [&mBall]( auto& mPaddle )
                                                                    {
                                                                        solvePaddleBallCollision( mPaddle, mBall );
                                                                    } );
                                       } );

                manager.refresh();

                manager.draw( renderer );

                // Update lives string and draw it.
                if (updateLives)
                {
                    std::string temp("Lives: " + std::to_string(remainingLives));
                    SDL_DestroyTexture(textLives);
                    textLives = createText(temp, font15, white, renderer);
                }
                renderTexture( textLives, renderer, 10, 10 );
            }

            //Update the screen
            SDL_RenderPresent( renderer );
        }


        // cleanup
        TTF_CloseFont( font15 );
        TTF_CloseFont( font35 );
        SDL_DestroyTexture( textState );
        SDL_DestroyTexture( textLives );
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_Quit();
    }

};

int main(int , char **)
{
    Game game;
    if (game.init() < 0)
    {
        return -1;
    }
    game.restart();
    game.run();
    return 0;
}

