An osx 10.11+ aimbot for cs2d 1.0.0.3. Only works on bots, but can easily be modified to work online by changing the tpl_get_bot signature to call tpl_get_player.

![aimbot](https://media.giphy.com/media/26n7ayhvYZlcxyDh6/giphy.gif)

## Compiling
Compiles as a dynamic library and needs to be injected into CS2D using an injector. Tested with the injector I wrote, available at https://github.com/attilathedud/dylib_injector and also with osxinj.

To compile, run `make`.

## About
cs2d stores your mouse angle in [0x3136e0]+0x14c as a degree from 0 - 275, and then from -90 - 0, but will downcast 275+ into its internal representation. To calculate the angle, we subtract the enemy's position from our own to get an absolute vector and then use the inverse tangent to calculate the angle.
```
               Enemy(x,y)
               /
              /
             /
            /
  Player(x,y)
```

1. Subtract the enemy's vector from our players:
```
               Enemy(x-player_x,y-player_y)
               / |
              /  | y-player_y
             /   |
            /Θ___|
  Player(0,0) x-player_x
```

2. To find Θ, use basic geometry: 
`tan(Θ) = opp / adj` or `Θ = tan^-1( y-player_y / x-player_x )`. Since the inverse tan is an unsigned operation (i.e., `tan^-1(45/45) = tan^-1(-45/45)`, we add 180 in cases where the enemy is to our left in order to calculate our degree correctly.

We then convert from radians to degrees using the formula: `(angle * 180.0 / M_PI)`. To deal with cs2d's internal conversion we add 90 to our result.

To determine the player to aim at, we calculate the Euclidean distance between our player and each enemy and aim at the closest one.

Our hook patches out the call that calculates how to spin our player so that we can directly write to those values.
