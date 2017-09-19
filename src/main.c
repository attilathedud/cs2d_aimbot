/*!
*   An osx 10.11+ aimbot for cs2d. Only works on bots, but can easily be modified to work online by 
*   changing the tpl_get_bot signature to call tpl_get_player.
*
*   Compiles as a dynamic library and needs to be injected into CS2D using an injector. Tested with the
*   injector I wrote, available at https://github.com/attilathedud/dylib_injector and also with osxinj.
*
*   To compile, run make.
*
*   cs2d stores your mouse angle in [0x3136e0]+0x14c as a degree from 0 - 275, and then from -90 - 0, but
*   will downcast 275+ into its internal representation. To calculate the angle, we subtract the enemy's
*   position from our own to get an absolute vector and then use the inverse tangent to calculate the angle.
*
*               Enemy(x,y)
*               /
*              /
*             /
*            /
*  Player(x,y)
*
*   1. Subtract the enemy's vector from our players:
*               Enemy(x-player_x,y-player_y)
*               / |
*              /  | y-player_y
*             /   |
*            /Θ___|
*  Player(0,0) x-player_x
*
*   2. To find Θ, use basic geometry: tan(Θ) = opp / adj or Θ = tan^-1( y-player_y / x-player_x ). Since
*   the inverse tan is an unsigned operation (i.e., tan^-1(45/45) = tan^-1(-45/45), we add 180 in cases 
*   where the enemy is to our left in order to calculate our degree correctly.
*
*   We then convert from radians to degrees using the formula: (angle * 180.0 / M_PI). To deal with cs2d's
*   internal conversion we add 90 to our result.
*
*   To determine the player to aim at, we calculate the Euclidean distance between our player and each
*   enemy and aim at the closest one.
*
*   Our hook patches out the call that calculates how to spin our player so that we can directly write to
*   those values.
*/

#include <math.h>
#include <mach/mach.h>

/*!
*   A helper function to call cs2d's internal _tpl_get_bot function located at (0x309168). It takes
*   an index between 2 - 32 and returns the base of the bot's struct in eax.
*
*   On no bot at that index, returns 0x3B6FE0.
*/ 
long tpl_get_bot( int index )
{
    long base_pointer = 0;

    __asm__ volatile(
        "mov %1, %%eax\n\t"             // move the index into eax
        "mov %%eax, (%%esp)\n\t"        // push the index to the top of the stack
        "mov $0x309168, %%eax\n\t"      // move the function's address into eax
        "mov (%%eax), %%eax\n\t"        // dereference it
        "call *%%eax\n\t"               // call dword ptr [0x309168]
        "mov %%eax, %0\n\t"             // move the result into the base_pointer
        :"=r"(base_pointer)
        :"r"(index)
    );

    return base_pointer;
}

/*!
*   Our hook that is run by the game. Loops through all the bots in the game and uses the method above 
*   to calculate our angle.
*/
void hook()
{
    // Save the registers and stack state
    __asm__( "pushal" );
    
    float closest_player_distance = -1.0;
    float closest_angle = 0.0;

    // Get our player's x and y
    long *client_struct = (long*)(0x3136e0);
    long player_struct = *client_struct;

    float *player_x = (float*)(player_struct+0x144);
    float *player_y = (float*)(player_struct+0x148);

    for( int i = 2; i < 32; i++ )
    {
        long bot_base_address = tpl_get_bot( i );
    
        if( bot_base_address == 0x3B6FE0 )
        {
            continue;
        }

        float *dx = (float*)(bot_base_address+0x144);
        float *dy = (float*)(bot_base_address+0x148);
    
        float abs_x = *dx - *player_x;
        float abs_y = *dy - *player_y;
    
        float angle = atanf( abs_y / abs_x );
        angle = (angle * 180.0 / M_PI) + 90;
    
        if( abs_x < 0 ) 
        {
            angle += 180;
        }

        // Calculate the distance between us and the bot
        float cur_closest_distance = sqrt( ( abs_x * abs_x ) + ( abs_y * abs_y ) );
        if( closest_player_distance == -1 || closest_player_distance > cur_closest_distance ) 
        {
            closest_player_distance = cur_closest_distance;
            closest_angle = angle;
        }
    }

    float *mouse_angle = (float*)(player_struct+0x14c);
    *mouse_angle = closest_angle;

    // Restore the registers and stack state
    __asm__( "popal" );
}

// Function run when the dylib is first injected. Patches our the call that calculates our turning and
// redirects it to our hook function.
void __attribute__ ((constructor)) install()
{
    unsigned int *patch_address = (unsigned int*)0x00105c32;

    vm_protect( mach_task_self(), (unsigned int)patch_address, 5, 0, VM_PROT_WRITE | VM_PROT_EXECUTE | VM_PROT_READ );

    *(patch_address) = (unsigned int)(&hook - 5) - (unsigned int)0x00105c31;
}
