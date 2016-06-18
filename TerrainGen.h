#pragma once
#include "ImprovedPerlin.h"
#include "Vector2D.h"
using SingleLayer = xtr::Vector2D<float>;

namespace ftg{ 
	class TerrainGen{
	public:
		void seed(std::string seed_string);
		void zeroTerrain(SingleLayer& map_in, short width, short height);
		void generateOceanFloor(SingleLayer& map_in, short width, short height, float slope, float roughness);
		void generateContinents(SingleLayer& map_in, short width, short height, float slope, float roughness, short numContinents);
		void makePeak(SingleLayer& map_in, short size_in, float slope, float roughness);
		void addHeightMap(SingleLayer& source, SingleLayer& destination, short source_size, short destination_width, short destination_height, bool cyclindrical, short x_offset, short y_offset, float scale);
		void fillHeightMap(SingleLayer& map_in, float roughness, short i, short run);
		void setSeaLevel(SingleLayer& the_map, float level, short width, short height);
		float getMaxValue(SingleLayer& map_in, short width, short height);
		float getMinValue(SingleLayer& map_in, short width, short height);
	private:
		float randomFloat(float min_val, float max_val);
		float averageSquare(SingleLayer& map_in, short x, short y, short d);
		float averageDiamond(SingleLayer& map_in, short x, short y, short d, short size_in, short options);
		float aveDevSquare(SingleLayer& map_in, short x, short y, short d, float average);
		float aveDevDiamond(SingleLayer& map_in, short x, short y, short d, short size_in, float average, short options);
		void calculateSquare(SingleLayer& map_in, short k, float rough, short run);
		void calculateDiamond(SingleLayer& map_in, short k, float rough, short run, short options);
		void generateHeightMap(SingleLayer& map_in, float slope, float roughness, short args, short run);
		void smoothHeightMap(SingleLayer& map_in, short width, short height, short passes);
		float seaCoverage(SingleLayer& map_in, float seaLevel, short width, short height);
		void adjustHeight(SingleLayer& map_in, short width, short height, float displacement);
		ImprovedPerlin perlin;
	};
}

