#include "TerrainGen.h"
using namespace ftg;

// pastes the content of one map onto another in an additive fashion
// **NOTE** that the source size plus offset must not exceed the destination size
void addToMap(SingleLayer& source, SingleLayer& destination, short x_offset, short y_offset, short source_size){
	for (short i = 0; i < source_size; i++)
		for (short j = 0; j < source_size; j++)
			destination[x_offset + i][y_offset + j] += source[i][j];
}

void TerrainGen::seed(std::string seed_string) {
    std::seed_seq seed_gen(seed_string.begin(), seed_string.end());
	perlin.setSeed_safe(seed_string);
	std::array<int, 10> new_seed;
	seed_gen.generate(new_seed.begin(), new_seed.end());
	srand(new_seed[0]);
}

void TerrainGen::zeroTerrain(SingleLayer& map_in, short width, short height) {
	for (int i = 0; i < width; i++)
		for (int j = 0; j < height; j++)
			map_in[i][j] = 0.0f;
}

void TerrainGen::generateOceanFloor(SingleLayer& map_in, short width, short height, float slope, float roughness) {
	for (int i = 0; i < width; i++)
		for (int j = 0; j < height; j++)
			for (int n : {1, 2, 4, 8, 16, 32})
				map_in[i][j] += perlin.noise2(float(i * n)/float(width), float(j * n)/float(height)) * slope;
}

/*generates the indicated number of Continents
 * map - the destination map
 * size - the size of the destination
 * slope - the max start peak
 * roughness - the divergence from average i.e the roughness
 * numContinents - the number of continents to generate */
void TerrainGen::generateContinents(SingleLayer& map_in, short width, short height, float slope, float roughness, short numContinents){
	// First calculate how many rows and columns to place the continents in plus their spacing and displacement
	short generated = 0;
	short xColumns;
	if ((int) sqrt((float) numContinents) == (int) sqrt((float) numContinents - 1))
		xColumns = (int) sqrt((float) numContinents) + 1;
	 else
		xColumns = (int) sqrt((float) numContinents);

	auto&& max = width < height ? width : height;
	short&& n = 1;
	while (1 << (n + 1) < max / xColumns)
		++n;
	short continent_size = (1 << n) + 1;
	short xSpacing = (width - 1) / xColumns;
	short ySpacing = 0;
	short yRows = 1;
	if (numContinents / xColumns == (numContinents - 1) / xColumns) {
		yRows = (numContinents / xColumns + 1);
		ySpacing = (height - 1) / (numContinents / xColumns + 1);
	} else {
		yRows = (numContinents / xColumns);
		ySpacing = (height - 1) / (numContinents / xColumns);
	}
	short ydisplacement = 0;

	// Now generate the continents
	// first allocate temporary memory for the continent
	SingleLayer continent(continent_size, continent_size);
	for (int i = 0; i < xColumns; i++) {
		// set the Y spacing and displacement if in the last row
		if (1 == xColumns - i && numContinents - generated != yRows) // if in the last row and not the amount left will not fill a complete column
				{
			ySpacing = (height) / (numContinents - generated);
			ydisplacement = ((height - 1) - (numContinents - generated) * continent_size) / 2;
		}
		for (int j = 0; j < yRows; j++) {
			if (generated < numContinents) {
				zeroTerrain(continent, continent_size, continent_size);
				generateHeightMap(continent, slope, roughness, 2, continent_size - 1);
				addToMap(continent, map_in, xSpacing * i + (-i), ydisplacement + ySpacing * j + (-j), continent_size);
				generated++;
			}
		}
	}
	// free temporary memory
	smoothHeightMap(map_in, width, height, 1);
}

void TerrainGen::makePeak(SingleLayer& map_in, short size_in, float slope, float roughness){
	zeroTerrain(map_in, size_in, size_in);
	generateHeightMap(map_in, slope, roughness, 3, size_in - 1);
}

void TerrainGen::addHeightMap(SingleLayer& source, SingleLayer& destination, short source_size, short destination_width, short destination_height, bool cyclindrical, short x_offset, short y_offset, float scale){
	short x_pos, y_pos;
	if (cyclindrical){
		for (short i = 0; i < source_size; i++){
			x_pos = x_offset + i;
			for (short j = 0; j < source_size; j++){
				y_pos = y_offset + j;
				if (y_pos >= 0 && y_pos < destination_height){  // only do points between edges of map
					if (x_pos > 0 && x_pos < destination_width - 1) destination[x_pos][y_pos] += source[i][j] * scale; // standard case
					else if (x_pos < 0) destination[x_pos + destination_width - 1][y_pos] += source[i][j] * scale;  // what to do if x is left of map
					else if (x_pos >= destination_width) destination[x_pos - (destination_width - 1)][y_pos] += source[i][j] * scale; // what to do if x is right of map
					else{ // x must be on one of the edges
						destination[0][y_pos] += source[i][j] * scale;
						destination[destination_width - 1][y_pos] += source[i][j] * scale;
					}
				}
			}
		}
	}
	else{
		for (short i = 0; i < source_size; i++){
			x_pos = x_offset + i;
			for (short j = 0; j < source_size; j++){
				y_pos = y_offset + j;
				if (y_pos >= 0 && y_pos < destination_height
					&& x_pos >= 0 && x_pos < destination_width) destination[x_pos][y_pos] += source[i][j] * scale; // only do points between edges of map
			}
		}
	}
}

void TerrainGen::generateHeightMap(SingleLayer& map_in, float slope, float rough, short args, short run)
/* Generates a height map using a fractal algorithm
 * SingleLayer& map - a 2D array of float data to store the map in
 * float slope - the max height of the seeding peek
 * float rough - the roughness of the map
 * short args - 0 = start by generating 4 corners with a cylindrical world;
 * 				1 = not cylindrical;
 * 				2 = start at center (will not be cylindrical since edges are predefined as 0.0)
 *				3 = like 2 but center peek will always be max (slope)
 * short run - how far away from top right corner to generate on must be n^2 */
{
	// sets the distance away from the last calculated vertices to calculate the next set
	short i;
	switch (args) {
	case 0:
		i = run / 2;
		map_in[run][0] = map_in[0][0] = randomFloat(0, slope);
		map_in[run][run] = map_in[0][run] = randomFloat(0, slope);
		break;
	case 1:
		i = run / 2;
		map_in[run][0] = randomFloat(0, slope);
		map_in[0][0] = randomFloat(0, slope);
		map_in[run][run] = randomFloat(0, slope);
		map_in[0][run] = randomFloat(0, slope);
		break;
	case 2:
		i = run / 4;
		map_in[run / 2][run / 2] += randomFloat(slope / 2, slope);
		args = 1;
		break;
	case 3:
		i = run / 4;
		map_in[run / 2][run / 2] += slope;
		args = 1;
		break;
	default:
		i = 0;
		break;
	}

	// Now generates terrain by randomly setting height using diamond and square method till no vertices are left to alter
	while (i > 0) {
		// Calculate squares
		calculateSquare(map_in, i, rough, run);

		// Calculate diamonds
		calculateDiamond(map_in, i, rough, run, args);
		i = i / 2;
	}
}

void TerrainGen::fillHeightMap(SingleLayer& map_in, float rough, short i, short run){
int args = 1;
	while (i > 0) {
        if (i == 1) rough = 0; // on the last pass smooth everything out.
		// Calculate squares
		calculateSquare(map_in, i, rough, run);

		// Calculate diamonds
		calculateDiamond(map_in, i, rough, run, args);
		i = i / 2;
	}
}

float TerrainGen::randomFloat(float min, float get_max) {
	float scale = RAND_MAX + 1.0f;
	float base = rand() / scale;
	float fine = rand() / scale;
	float random = (base + fine / scale);
	return ((((float) fabs(random)) * (get_max - min)) + min);
}

void TerrainGen::calculateSquare(SingleLayer& map_in, short k, float roughness, short run) {
	float average;
	float avgDev2;
	for (int i = 0; i <  run; i += 2 * k) {
		for (int j = 0; j < run; j += 2 * k) {
			average = averageSquare(map_in, (k + i), (k + j), k);
			avgDev2 = 2.0f * aveDevSquare(map_in, (k + i), (k + j), k, average);
			map_in[k + i][k + j] = average + roughness * randomFloat(-avgDev2, avgDev2);
		}
	}
}

void TerrainGen::calculateDiamond(SingleLayer& map_in, short k, float roughness, short run, short options) {
	float average;
	float avgDev2;
	for (int i = 0; i < run; i += 2 * k) {
		for (int j = 0; j < run; j += 2 * k) {
			// This condition is for calculating the average in case it lies on the edge of an interior region
			average = averageDiamond(map_in, (k + i), (j), k, run, options);
			avgDev2 = 2.0f * aveDevDiamond(map_in, (k + i), (j), k, run, average, options);
			if (avgDev2 > 0) {
				map_in[k + i][j] = average + roughness * randomFloat(-avgDev2, avgDev2);
			}
			if (options == 0) {
				if (j == 0)
					map_in[k + i][run] = map_in[k + i][j]; // if on the edge make oposite edge the same  This is for the polar edge not entirely necessary but allows for "donut" worlds
				else if (j == run)
					map_in[k + i][0] = map_in[k + i][j]; // if on the edge make oposite edge the same
				// k + i can never be size so don't worry about that.  (size is even k + 2k*n can never be size since size = k*2^n)
				// k + i can never be zero so don't worry about that. (k >= 1)
			}
		}
	}

	for (int i = 0; i < run; i += 2 * k) {
		for (int j = 0; j < run; j += 2 * k) {
			average = averageDiamond(map_in, (i), (k + j), k, run, options);
			avgDev2 = 2.0f * aveDevDiamond(map_in, (i), (k + j), k, run, average, options);
			if (avgDev2 > 0) {
				map_in[i][k + j] = average + roughness * randomFloat(-avgDev2, avgDev2);
			}
			if (options == 0) {
				if (i == 0)
					map_in[run][k + j] = map_in[i][k + j]; // if on the edge make the oposite edge the same
				if (i == run)
					map_in[0][k + j] = map_in[i][k + j]; // if on the edge make the oposite edge the same
				// same reasons as for j not to worry about j = 0 or size here.
			}
		}
	}
}

float TerrainGen::averageSquare(SingleLayer& map_in, short x, short y, short d) {
	float average = ((map_in[x + d][y + d] + map_in[x - d][y + d] + map_in[x + d][y - d] + map_in[x - d][y - d]) / 4.0f);

	return average;
}

float TerrainGen::aveDevSquare(SingleLayer& map_in, short x, short y, short d, float average) {
	float averageDev = ((fabs(map_in[x + d][y + d] - average) + fabs(map_in[x - d][y + d] - average) + fabs(map_in[x + d][y - d] - average)
			+ fabs(map_in[x - d][y - d] - average)) / 4.0f);

	return averageDev;
}

float TerrainGen::averageDiamond(SingleLayer& map_in, short x, short y, short d, short run, short options) {
	float average = 0;
	if (options == 0) {
		if (y == 0) {
			average = ((map_in[x + d][y] + map_in[x - d][y] + map_in[x][y + d] + map_in[x][run - d]) / 4.0f);
		} else if (y == run) {
			average = ((map_in[x + d][y] + map_in[x - d][y] + map_in[x][0 + d] + map_in[x][y - d]) / 4.0f);
		} else if (x == 0) {
			average = ((map_in[x + d][y] + map_in[run - d][y] + map_in[x][y + d] + map_in[x][y - d]) / 4.0f);
		} else if (x == run) {
			average = ((map_in[0 + d][y] + map_in[x - d][y] + map_in[x][y + d] + map_in[x][y - d]) / 4.0f);
		} else {
			average = ((map_in[x + d][y] + map_in[x - d][y] + map_in[x][y + d] + map_in[x][y - d]) / 4.0f);
		}
	} else if (options == 1) {
		if (y == 0) {
			average = 0;
		} else if (y == run) {
			average = 0;
		} else if (x == 0) {
			average = 0;
		} else if (x == run) {
			average = 0;
		} else {
			average = ((map_in[x + d][y] + map_in[x - d][y] + map_in[x][y + d] + map_in[x][y - d]) / 4.0f);
		}
	}
	return average;
}

float TerrainGen::aveDevDiamond(SingleLayer& map_in, short x, short y, short d, short run, float average, short options) {
	float averageDev = 0.0f;
	if (options == 0) {
		if (y == 0) // The follow 4 conditions deal with regions that are on the edge of the world map to allow for wrapping to the other side
				{
			averageDev = ((fabs(map_in[x + d][y] - average) + fabs(map_in[x - d][y] - average) + fabs(map_in[x][y + d] - average) + fabs(map_in[x][run - d] - average))
					/ 4.0f);
		} else if (y == run) {
			averageDev =
					((fabs(map_in[x + d][y] - average) + fabs(map_in[x - d][y] - average) + fabs(map_in[x][0 + d] - average) + fabs(map_in[x][y - d] - average)) / 4.0f);
		} else if (x == 0) {
			averageDev = ((fabs(map_in[x + d][y] - average) + fabs(map_in[run - d][y] - average) + fabs(map_in[x][y + d] - average) + fabs(map_in[x][y - d] - average))
					/ 4.0f);
		} else if (x == run) {
			averageDev =
					((fabs(map_in[0 + d][y] - average) + fabs(map_in[x - d][y] - average) + fabs(map_in[x][y + d] - average) + fabs(map_in[x][y - d] - average)) / 4.0f);
		} else {
			averageDev =
					((fabs(map_in[x + d][y] - average) + fabs(map_in[x - d][y] - average) + fabs(map_in[x][y + d] - average) + fabs(map_in[x][y - d] - average)) / 4.0f);
		}
	} else if (options == 1) {
		// The follow 4 conditions deal with regions that are on the edge of the defined run
		if (y == 0) {
			averageDev = 0;
		} else if (y == run) {
			averageDev = 0;
		} else if (x == 0) {
			averageDev = 0;
		} else if (x == run) {
			averageDev = 0;
			// this calculates it for the rest
		} else {
			averageDev =
					((fabs(map_in[x + d][y] - average) + fabs(map_in[x - d][y] - average) + fabs(map_in[x][y + d] - average) + fabs(map_in[x][y - d] - average)) / 4.0f);
		}
	}
	return averageDev;
}

// Smooth the height map
void TerrainGen::smoothHeightMap(SingleLayer& map_in, short width, short height, short passes) {
	// Allocate memory for temporary map
	SingleLayer height_map(width, height);

	float sum = 0;
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			sum = 0.0f;
			// position to the left
			if (i == 0)
				sum += map_in[width - 1][j];
			 //if is on left edge
			else
				sum += map_in[i - 1][j];
			if (i == 0 && j == 0)
				sum += map_in[width - 1][height - 1]; //if is on left top corner
			else if (i == 0)
				sum += map_in[width - 1][j - 1]; // if is on left edge
			else if (j == 0)
				sum += map_in[i - 1][height - 1]; // is on the top edge
			else
				sum += map_in[i - 1][j - 1];
			//position to the top
			if (j == 0)
				sum += map_in[i][height - 1]; //is on the top edge
			else
				sum += map_in[i][j - 1];
			// position to the top right
			if (i == width - 1 && j == 0)
				sum += map_in[0][height - 1]; //if is on left right corner
			else if (j == 0)
				sum += map_in[i + 1][height - 1]; // if is on the top edge
			else if (i == width - 1)
				sum += map_in[0][j - 1]; // if is on the right edge
			else
				sum += map_in[i + 1][j - 1];
			//position to the right
			if (i == width - 1)
				sum += map_in[0][j]; //is on the top edge
			else
				sum += map_in[i + 1][j];
			// position to the bottom right
			if (i == width - 1 && j == height - 1)
				sum += map_in[0][0]; //if is on bottom right corner
			else if (j == height - 1)
				sum += map_in[i + 1][0]; // if is on bottom edge
			else if (i == width - 1)
				sum += map_in[0][j + 1]; // is on the right edge
			else
				sum += map_in[i + 1][j + 1];
			// position on the bottom
			if (j == height - 1)
				sum += map_in[i][0]; //is on the bottom edge
			else
				sum += map_in[i][j + 1];
			// poistion on the bottom left
			if (i == 0 && j == height - 1)
				sum += map_in[width - 1][0]; //if is on bottom right corner
			else if (j == height - 1)
				sum += map_in[i - 1][0]; // if is on bottom edge
			else if (i == 0)
				sum += map_in[0][j + 1]; // if is on the left edge
			else
				sum += map_in[i - 1][j + 1];
			sum += map_in[i][j]; // Sum it's self as well
			height_map[i][j] = sum / 9.0f;
		}
	}
	// swap values
	for (int i = 0; i < width; i++)
		for (int j = 0; j < height; j++)
			map_in[i][j] = height_map[i][j];
}

float TerrainGen::getMaxValue(SingleLayer& map_in, short width, short height){
    float maxHeight = map_in[0][0];
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            if (map_in[i][j] > maxHeight)
                maxHeight = map_in[i][j];
    return maxHeight;
}

float TerrainGen::getMinValue(SingleLayer& map_in, short width, short height){
    float minHeight = map_in[0][0];
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            if (map_in[i][j] < minHeight)
                minHeight = map_in[i][j];
    return minHeight;
}

void TerrainGen::setSeaLevel(SingleLayer& the_map, float level, short width, short height){
// Sets seaLevel to the given percentage and shifts map so that sea level is at 0.0f
    //Calculate average, min and max height
    auto&& totalHeight = 0.0f;
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            totalHeight += the_map[i][j];

    auto&& averageHeight = totalHeight / (float) (width * height);
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            the_map[i][j] -= averageHeight;

    auto&& maxHeight = getMaxValue(the_map, width, height);
    auto&& minHeight = getMinValue(the_map, width, height);

    auto&& seaLevel = level * (maxHeight - minHeight) + minHeight - averageHeight; // Set a good approximated sealevel based on height percentage
    if (seaCoverage(the_map, seaLevel, width, height) > level)
        while (seaCoverage(the_map, seaLevel, width, height) > level)
            seaLevel--;

    else if (seaCoverage(the_map, seaLevel, width, height) < level)
        while (seaCoverage(the_map, seaLevel, width, height) < level)
            seaLevel++;

	adjustHeight(the_map, width, height, -seaLevel);
}

float TerrainGen::seaCoverage(SingleLayer& the_map, float seaLevel, short width, short height){
    auto&& underWater = 0;
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            if (the_map[i][j] < seaLevel) underWater++;
    auto&& percentUnder = (float) underWater / ((float) width * (float) height);
    return percentUnder;
}

void TerrainGen::adjustHeight(SingleLayer& map_in, short width, short height, float displacement){
	for (short i = 0; i < width; ++i)
		for (short j = 0; j < height; ++j)
			map_in[i][j] += displacement;
}
