# Add this board to the list of buildable boards
ALL_BOARDS += flyingpio

# Set the cpu architecture here that matches your STM32
flyingpio_cpuarch := f0

# Short name of this board (used to display board name in parallel builds)
# Should be exactly 4 characters long.
flyingpio_short := 'fpio'

flyingpio_bootloader := no
