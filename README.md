[![CodeFactor](https://www.codefactor.io/repository/github/zaimoni/cataclysm/badge)](https://www.codefactor.io/repository/github/zaimoni/cataclysm)

Cataclysm is a post-apocalyptic roguelike, set in the countryside of New England
after a devastating plague of monsters and zombies.

[2018] At present time, Cataclysm is still in early alpha.  This fork of Whales' Cataclysm 
is not expected to be developed rapidly; intent is to get a working baseline that 
compiles with Microsoft Visual Studio 2017 as a baseline.

Source for this fork is available at the github repository, http://github.com/zaimoni/Cataclysm .
The contents of the code_doc and data directories are as they were in 2013.

[2019-11-05] If there are missing DLLs with the Windows build, download them from Microsoft.

[2013] Compiling Cataclysm under linux should remain straightforward, and only requires the
ncurses development libraries.  Under Ubuntu, these libraries can be found in
the libncurses5-dev package ("sudo apt-get install libncurses5-dev" will install
this package).

Cataclysm is very different from most roguelikes in many ways.  Rather than
being set in a vertical, linear dungeon, it is set in an unbounded, 3D world.
This means that exploration plays a much bigger role than in most roguelikes,
and the game is much less linear.
Because the map is so huge, it is actually completely persistent between games.
If you die, and start a new character, your new game will be set in the same
game world as your last.  Like in many roguelikes, you will be able to loot the
dead bodies of previous characters; unlike most roguelikes, you will also be
able to retrace their steps completely, and any dramatic changes made to the
world will persist into your next game.
While this makes for interesting depth of play, and the ability to "save" game
progress even after death, some prefer to start each game with a freshly
generated world.  This can be achieved by erasing the contents of the save
directory, found in the same folder as the executable.  "rm save/*" will erase
these files for you.

Cataclysm's gameplay also includes many unique quirks, and the learning curve is
somewhat steep, even for those experienced with roguelikes.  Included with the
game is a tutorial which runs the player through most of the key features.  The
game also has extensive documentation via the ? command.  Pressing ?1 will list
all the key commands, which is a good place to start.

Feature requests will be noted, but are unlikely to be acted on before the MSVC++
build is working.  They are more likely to reach me at the repository than otherwise.
