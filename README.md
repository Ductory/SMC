# Symbol Modifier for COFF (SMC)

SMC (Symbol Modifier for COFF) is a utility designed to facilitate the handling of different symbol naming conventions and mangling rules imposed by various compilers in mixed programming environments. It allows for the renaming of symbol names within COFF (Common Object File Format) files, ensuring compatibility and ease of integration across different programming languages and their compilers.

## Usage
    smc infile outfile old new [old new ...]

where:

    infile      is the name of the input COFF file.
    outfile     is the name for the output COFF file with modified symbols.
    old new     is a pair where `old` is the original symbol name to be modified and 
                `new` is the new symbol name.
    @listfile   is an optional argument where `listfile` is a file containing multiple
                'old new' pairs.

## Example
`smc program.o program_mod.o test testFunction`
- This command will modify the symbol 'test' to 'testFunction' in the 'program.o' file and output the result to 'program_mod.o'.

`smc program.o program_mod.o @symbols.txt`
- This command will read 'old new' pairs from 'symbols.txt' and apply them to 'program.o', resulting in 'program_mod.o'.

## Note
This tool is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

## License
This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

Dangfer.
