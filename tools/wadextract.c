/**
 * WAD file extractor for libMusDoom testing
 * 
 * Extracts MUS music files and GENMIDI instruments from Doom WAD files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// WAD file header
typedef struct {
    char identification[4];     // "IWAD" or "PWAD"
    int32_t num_lumps;
    int32_t info_table_offset;
} wad_header_t;

// WAD lump entry
typedef struct {
    int32_t file_pos;
    int32_t size;
    char name[8];
} wad_lump_t;

// Read a WAD file and extract lumps
int main(int argc, char* argv[]) {
    FILE* wad_file;
    wad_header_t header;
    wad_lump_t* lumps;
    int i;
    
    if (argc < 2) {
        printf("Usage: %s <wadfile> [lumpname]\n", argv[0]);
        printf("  Extracts lumps from a Doom WAD file.\n");
        printf("  If no lumpname is specified, lists all lumps.\n");
        return 1;
    }
    
    wad_file = fopen(argv[1], "rb");
    if (!wad_file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", argv[1]);
        return 1;
    }
    
    // Read header
    if (fread(&header, sizeof(header), 1, wad_file) != 1) {
        fprintf(stderr, "Error: Cannot read WAD header\n");
        fclose(wad_file);
        return 1;
    }
    
    // Verify WAD type
    if (memcmp(header.identification, "IWAD", 4) != 0 &&
        memcmp(header.identification, "PWAD", 4) != 0) {
        fprintf(stderr, "Error: Not a valid WAD file\n");
        fclose(wad_file);
        return 1;
    }
    
    printf("WAD Type: %.4s\n", header.identification);
    printf("Num Lumps: %d\n", header.num_lumps);
    printf("Info Table Offset: %d\n\n", header.info_table_offset);
    
    // Read lump directory
    lumps = (wad_lump_t*)malloc(header.num_lumps * sizeof(wad_lump_t));
    if (!lumps) {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(wad_file);
        return 1;
    }
    
    fseek(wad_file, header.info_table_offset, SEEK_SET);
    if (fread(lumps, sizeof(wad_lump_t), header.num_lumps, wad_file) != (size_t)header.num_lumps) {
        fprintf(stderr, "Error: Cannot read lump directory\n");
        free(lumps);
        fclose(wad_file);
        return 1;
    }
    
    // If no lump name specified, list all lumps
    if (argc < 3) {
        printf("Lumps in WAD:\n");
        for (i = 0; i < header.num_lumps; i++) {
            char name[9] = {0};
            memcpy(name, lumps[i].name, 8);
            printf("  %4d: %-8s  size: %d\n", i, name, lumps[i].size);
        }
        free(lumps);
        fclose(wad_file);
        return 0;
    }
    
    // Find and extract the specified lump
    for (i = 0; i < header.num_lumps; i++) {
        char name[9] = {0};
        memcpy(name, lumps[i].name, 8);
        
        if (strcasecmp(name, argv[2]) == 0) {
            FILE* out_file;
            uint8_t* data;
            char out_name[256];
            
            printf("Found lump '%s' at index %d, size %d\n", name, i, lumps[i].size);
            
            // Read lump data
            data = (uint8_t*)malloc(lumps[i].size);
            if (!data) {
                fprintf(stderr, "Error: Out of memory\n");
                free(lumps);
                fclose(wad_file);
                return 1;
            }
            
            fseek(wad_file, lumps[i].file_pos, SEEK_SET);
            if (fread(data, 1, lumps[i].size, wad_file) != (size_t)lumps[i].size) {
                fprintf(stderr, "Error: Cannot read lump data\n");
                free(data);
                free(lumps);
                fclose(wad_file);
                return 1;
            }
            
            // Write to file
            snprintf(out_name, sizeof(out_name), "%s.lmp", name);
            out_file = fopen(out_name, "wb");
            if (!out_file) {
                fprintf(stderr, "Error: Cannot create output file\n");
                free(data);
                free(lumps);
                fclose(wad_file);
                return 1;
            }
            
            fwrite(data, 1, lumps[i].size, out_file);
            fclose(out_file);
            
            printf("Extracted to '%s'\n", out_name);
            
            free(data);
            free(lumps);
            fclose(wad_file);
            return 0;
        }
    }
    
    fprintf(stderr, "Error: Lump '%s' not found\n", argv[2]);
    free(lumps);
    fclose(wad_file);
    return 1;
}
