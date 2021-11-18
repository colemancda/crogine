#### Course File Format
Courses are defined by sets of two different kinds of file (subject to change)

###### Course File
A course folder should contain one file with the name `course.data`. It has the following format:

    course course_01 //name or identifier, doesn't have to be unique
    {
        skybox = "assets/golf/images/skybox/sky.ccm" //a path to the skybox to use when the course is loaded
        billboard_model = "assets/golf/models/shrubbery.cmt" //path to the model definition for the billboard file containing the trees/shrubs
        billboard_sprites = "assets/golf/sprites/shrubbery.spt" //path to the spritesheet containing the sprite definitions for each billboard in the billboard model
        grass = 1,0.5,0,1 //surrounding grass colour. This is optional and will default to dark green from the colordome-32 palette

        title = "St. Billybob's links, Isle of Wibble" //course title, shown in the game lobby
        description = "This is a test course. Why not have a play?" //a brief description, shown in the game lobby


        //a list of paths to *.hole files. These should be in the order in which the holes are to be played.
        hole = "assets/golf/courses/course_01/01.hole"
        hole = "assets/golf/courses/course_01/02.hole"
    }

The course file contains a list of paths to *.hole files which describe the individual holes that make up a course. This way holes can be recycled thoughout different course layouts should it be so desired. Up to 18 holes can be added to a course (any further holes will be ignored when the course file is loaded).

###### Hole file
Hole files describe which models/assets make up the hole, as well as the position of the tee, the hole and the par for that hole. Optionally prop objects can be added to the hole to display models as props in the game. Crowd objects can also optionally be added to place a small crowd of spectators. By default crowds are aligned left to right along the X axis and can be rotated with the `rotation` property.

    hole 01 //hole ID
    {
        map = "assets/golf/holes/01.png" //path to map file, see below
        model = "assets/golf/models/hole_01.cmt" //path to the hole model
        pin = 22, 0, -172 //world coordinates on the map file to the hole.
        target = 22, 0, -172 //initial direction which the player should be looking when the hole is loaded. Usually the pin, but can be different if hole is sharp dogleg
        tee = 243, 2.5, -30 //world coordinates on the map file to the tee
        par = 3 //par for this hole.

        prop
        {
            position = 70,0,45 //position in world units
            rotation = 45 //rotation around the Y axis (in degrees)
            model = "assets/golf/models/cart.cmt" //path to a model description to use as a prop
        }

        crowd
        {
            position = -130, 0, -65 //position of the left-most crowd member in world units
            rotation = 180 //rotation about the Y axis at the left-most point
        }
    }


Hole file creation can be aided with the export script for blender. See readme.txt for more details.

The map file contains metadata about the hole stored as an image. The image is stored with a scale of one pixel per metre. In other words each pixel represents one metre square on the course grid. The XY coordinates of the image, starting at the bottom left, are mapped to the X, -Z coordinates of the 3D world. Map sizes are fixed at 320 pixels by 200 pixels.

The red channel of the map file stores a value representing the current terrain type. This is used by the client to decide where foliage billboards are placed. Pixels with a value between 50 and 59 will have foliage rendered on them. It is important to paint this value carefully as excessive foliage can affect performance, as well as obscure player views. Normally foliage would appear only around the border of the hole model, any further away and the billboards are not visible and are technically wasted. Remaining pixels should be filled with the value 45.

The green channel of the map contains height values used to create the surrounding terrain. Values of zero will be below the water plane, whereas values of 255 will raise the terrain at that point to its max height (currently 4.5 metres). A heightmap can be created by sculpting a 3D plane and baking its values to a texture in software suxh as Blender. See readme.txt for more details.