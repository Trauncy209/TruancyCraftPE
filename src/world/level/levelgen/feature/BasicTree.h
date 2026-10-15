#ifndef NET_MINECRAFT_WORLD_LEVEL_LEVELGEN_FEATURE__BasicTree_H__
#define NET_MINECRAFT_WORLD_LEVEL_LEVELGEN_FEATURE__BasicTree_H__

//package net.minecraft.world.level.levelgen.feature;

#include "Feature.h"

#include "../../../../util/Random.h"
#include "../../Level.h"

#include "../../tile/TreeTile.h"

class Level;

class BasicTree : public Feature
{
	typedef Feature super;
private:



	unsigned char axisConversionArray[6];
	Random *rnd;
	Level *thisLevel;
	int origin[3];
	int height;
	int trunkHeight;
	float trunkHeightScale;
	float branchDensity;
	float branchSlope;
	float widthScale;
	float foliageDensity;
	int trunkWidth;
	int heightVariance;
	int minimumHeight;
	int baseHeightVariance;
	float baseWidthScale;
	float baseFoliageDensity;
	int foliageHeight;
	int **foliageCoords;
	int foliageCoordsLength;
	void prepare(){
		trunkHeight = (int) (height * trunkHeightScale);
		if (trunkHeight >= height) trunkHeight = height - 1;
		int clustersPerY = (int) (1.382f + pow(foliageDensity * height / 13.0, 2));
		if (clustersPerY < 1) clustersPerY = 1;
		int **tempFoliageCoords = new int *[clustersPerY * height];
		for( int i = 0; i < clustersPerY * height; i++ )
		{
			tempFoliageCoords[i] = new int[4];
		}
		int y = origin[1] + height - foliageHeight;
		int clusterCount = 1;
		int trunkTop = origin[1] + trunkHeight;
		int relativeY = y - origin[1];

		tempFoliageCoords[0][0] = origin[0];
		tempFoliageCoords[0][1] = y;
		tempFoliageCoords[0][2] = origin[2];
		tempFoliageCoords[0][3] = trunkTop;
		y--;

		while (relativeY >= 0)
		{
			int num = 0;

			float shapefac = treeShape(relativeY);
			if (shapefac < 0)
			{
				y--;
				relativeY--;
				continue;
			}

			float originOffset = 0.5f;
			while (num < clustersPerY)
			{
				float radius = widthScale * (shapefac * (rnd->nextFloat() + 0.328f));
				float angle = rnd->nextFloat() * 2.0f * 3.14159f;
				int x = Mth::floor(radius * sin(angle) + origin[0] + originOffset);
				int z = Mth::floor(radius * cos(angle) + origin[2] + originOffset);
				int checkStart[] = { x, y, z };
				int checkEnd[] = { x, y + foliageHeight, z };
				if (checkLine(checkStart, checkEnd) == -1) {
					int checkBranchBase[] = { origin[0], origin[1], origin[2] };
					float distance = sqrt(pow(abs(origin[0] - checkStart[0]), 2.0f) + pow(abs(origin[2] - checkStart[2]), 2.0f));
					float branchHeight = distance * branchSlope;
					if ((checkStart[1] - branchHeight) > trunkTop)
					{
						checkBranchBase[1] = trunkTop;
					}
					else
					{
						checkBranchBase[1] = (int) (checkStart[1] - branchHeight);
					}
					if (checkLine(checkBranchBase, checkStart) == -1)
					{
						tempFoliageCoords[clusterCount][0] = x;
						tempFoliageCoords[clusterCount][1] = y;
						tempFoliageCoords[clusterCount][2] = z;
						tempFoliageCoords[clusterCount][3] = checkBranchBase[1];
						clusterCount++;
					}
				}
				num++;
			}
			y--;
			relativeY--;
		}

		foliageCoordsLength = clusterCount;
		foliageCoords = tempFoliageCoords;

		for( int i = clusterCount; i < clustersPerY * height; i++ )
		{
			delete [] tempFoliageCoords[i];
			tempFoliageCoords[i] = NULL;
		}


	}

	void crossection(int x, int y, int z, float radius, unsigned char direction, int material)
	{

		int rad = (int) (radius + 0.618);
		unsigned char secidx1 = axisConversionArray[direction];
		unsigned char secidx2 = axisConversionArray[direction + 3];
		int center[] = { x, y, z };
		int position[] = { 0, 0, 0 };
		int offset1 = -rad;
		int offset2 = -rad;
		int thismat;
		position[direction] = center[direction];
		while (offset1 <= rad)
		{
			position[secidx1] = center[secidx1] + offset1;
			offset2 = -rad;
			while (offset2 <= rad)
			{
				float off1 = (float)offset1 + 0.5f;
				float off2 = (float)offset2 + 0.5f;
				float thisdistance = (off1 * off1) + (off2 * off2);
				if (thisdistance > radius * radius)
				{
					offset2++;
					continue;
				}
				position[secidx2] = center[secidx2] + offset2;

				thismat = thisLevel->getTile(position[0], position[1], position[2]);

				if (!((thismat == 0) || (thismat == Tile::leaves->id)))
				{
					offset2++;
					continue;
				}

				placeBlock(thisLevel, position[0], position[1], position[2], material, 0);

				offset2++;
			}
			offset1++;
		}

	}
	float treeShape(int y){
		if (y < (((float) height) * 0.3f)) return (float) -1.618f;
		float radius = ((float) height) / ((float) 2.0f);
		float adjacent = (((float) height) / ((float) 2.0f)) - y;
		float distance;
		if (adjacent == 0) distance = radius;
		else if (abs(adjacent) >= radius) distance = (float) 0.0f;
		else distance = (float) sqrt(pow(abs(radius), 2) - pow(abs(adjacent), 2));
		distance *= (float) 0.5f;
		return distance;
	}
	float foliageShape(int y){
		if ((y < 0) || (y >= foliageHeight)) return (float) -1;
		else if ((y == 0) || (y == (foliageHeight - 1))) return (float) 2;
		else return (float) 3;
	}
	void foliageCluster(int x, int y, int z){

		int topy = y + foliageHeight;
		int cury = topy - 1;
		float radius;
		while (cury >= y)
		{
			radius = foliageShape(cury - y);
			crossection(x, cury, z, radius, (unsigned char) 1, Tile::leaves->id);
			cury--;
		}

	}
	void limb(int *start, int *end, int material)
	{
		int delta[] = { 0, 0, 0 };
		unsigned char idx = 0;
		unsigned char primidx = 0;
		while (idx < 3)
		{
			delta[idx] = end[idx] - start[idx];
			if (abs(delta[idx]) > abs(delta[primidx]))
			{
				primidx = idx;
			}
			idx++;
		}
		if (delta[primidx] == 0) return;
		unsigned char secidx1 = axisConversionArray[primidx];
		unsigned char secidx2 = axisConversionArray[primidx + 3];
		int primsign;
		if (delta[primidx] > 0) primsign = 1;
		else primsign = -1;
		float secfac1 = ((float) delta[secidx1]) / ((float) delta[primidx]);
		float secfac2 = ((float) delta[secidx2]) / ((float) delta[primidx]);
		int coordinate[] = { 0, 0, 0 };
		int primoffset = 0;
		int endoffset = delta[primidx] + primsign;
		while (primoffset != endoffset)
		{
			coordinate[primidx] = Mth::floor(start[primidx] + primoffset + 0.5);
			coordinate[secidx1] = Mth::floor(start[secidx1] + (primoffset * secfac1) + 0.5);
			coordinate[secidx2] = Mth::floor(start[secidx2] + (primoffset * secfac2) + 0.5);

			int dir = 0;
			int xdiff = abs(coordinate[0] - start[0]);
			int zdiff = abs(coordinate[2] - start[2]);
			int maxdiff = (std::max)(xdiff, zdiff);

			if (maxdiff > 0)
			{
				if (xdiff == maxdiff)
				{
					dir = 0;
				}
				else if (zdiff == maxdiff)
				{
					dir = 0;
				}
			}
			placeBlock(thisLevel, coordinate[0], coordinate[1], coordinate[2], material, dir);
			primoffset += primsign;
		}
	}
	void makeFoliage(){
		int idx = 0;
		int finish = foliageCoordsLength;
		while (idx < finish)
		{
			int x = foliageCoords[idx][0];
			int y = foliageCoords[idx][1];
			int z = foliageCoords[idx][2];
			foliageCluster(x, y, z);
			idx++;
		}
	}
	bool trimBranches(int localY){
		if (localY < (height * 0.2)) return false;
		else return true;
	}
	void makeTrunk(){
		int x = origin[0];
		int startY = origin[1];
		int topY = origin[1] + trunkHeight;
		int z = origin[2];
		int startCoord[] = { x, startY, z };
		int endCoord[] = { x, topY, z };
		limb(startCoord, endCoord, Tile::treeTrunk->id);
		if (trunkWidth == 2)
		{
			startCoord[0] += 1;
			endCoord[0] += 1;
			limb(startCoord, endCoord, Tile::treeTrunk->id);
			startCoord[2] += 1;
			endCoord[2] += 1;
			limb(startCoord, endCoord, Tile::treeTrunk->id);
			startCoord[0] += -1;
			endCoord[0] += -1;
			limb(startCoord, endCoord, Tile::treeTrunk->id);
		}
	}
	void makeBranches(){
		int idx = 0;
		int finish = foliageCoordsLength;
		int baseCoord[] = { origin[0], origin[1], origin[2] };
		while (idx < finish)
		{
			int *coordValues = foliageCoords[idx];
			int endCoord[] = { coordValues[0], coordValues[1], coordValues[2] };
			baseCoord[1] = coordValues[3];
			int localY = baseCoord[1] - origin[1];
			if (trimBranches(localY))
			{
				limb(baseCoord, endCoord, Tile::treeTrunk->id);
			}
			idx++;
		}
	}
	int checkLine(int *start, int *end){

		int delta[] = { 0, 0, 0 };
		unsigned char idx = 0;
		unsigned char primidx = 0;
		while (idx < 3)
		{
			delta[idx] = end[idx] - start[idx];
			if (abs(delta[idx]) > abs(delta[primidx]))
			{
				primidx = idx;
			}
			idx++;
		}
		if (delta[primidx] == 0) return -1;
		unsigned char secidx1 = axisConversionArray[primidx];
		unsigned char secidx2 = axisConversionArray[primidx + 3];
		int primsign;
		if (delta[primidx] > 0) primsign = 1;
		else primsign = -1;
		float secfac1 = ((float) delta[secidx1]) / ((float) delta[primidx]);
		float secfac2 = ((float) delta[secidx2]) / ((float) delta[primidx]);
		int coordinate[] = { 0, 0, 0 };
		int primoffset = 0;
		int endoffset = delta[primidx] + primsign;
		int thismat;
		while (primoffset != endoffset)
		{
			coordinate[primidx] = start[primidx] + primoffset;
			coordinate[secidx1] = Mth::floor(start[secidx1] + (primoffset * secfac1));
			coordinate[secidx2] = Mth::floor(start[secidx2] + (primoffset * secfac2));
			thismat = thisLevel->getTile(coordinate[0], coordinate[1], coordinate[2]);
			if (!((thismat == 0) || (thismat == Tile::leaves->id)))
			{

				break;
			}
			primoffset += primsign;
		}

		if (primoffset == endoffset)
		{
			return -1;
		}

		else
		{
			return abs(primoffset);
		}
	}
	bool checkLocation(){

		int startPosition[] = { origin[0], origin[1], origin[2] };
		int endPosition[] = { origin[0], origin[1] + height - 1, origin[2] };


		int baseMaterial = thisLevel->getTile(origin[0], origin[1] - 1, origin[2]);
		if (!((baseMaterial == 2) || (baseMaterial == 3)))
		{
			return false;
		}
		int allowedHeight = checkLine(startPosition, endPosition);
		if (allowedHeight == -1)
		{
			return true;
		}

		else if (allowedHeight < (std::max)(6, minimumHeight))
		{
			return false;
		}
		else
		{
			height = allowedHeight;
			return true;
		}
	}

public:
	BasicTree(bool doUpdate, int minHeight = 5, int heightRange = 12, float canopyWidth = 1.0f, float canopyDensity = 1.0f) : Feature(doUpdate){
		minimumHeight = (std::max)(5, minHeight);
		baseHeightVariance = (std::max)(1, heightRange);
		baseWidthScale = canopyWidth;
		baseFoliageDensity = canopyDensity;
		axisConversionArray[0] = 2;
		axisConversionArray[1] = 0;
		axisConversionArray[2] = 0;
		axisConversionArray[3] = 1;
		axisConversionArray[4] = 2;
		axisConversionArray[5] = 1;
		rnd = new Random();
		origin[0] = 0;
		origin[1] = 0;
		origin[2] = 0;
		height = 0;
		trunkHeight = 0;
		trunkHeightScale = 0.618;
		branchDensity = 1.0;
		branchSlope = 0.381;
		widthScale = baseWidthScale;
		foliageDensity = baseFoliageDensity;
		trunkWidth = 1;
		heightVariance = baseHeightVariance;
		foliageHeight = 4;
		foliageCoords = NULL;
		foliageCoordsLength = 0;
	}
	virtual ~BasicTree(){
		delete rnd;

		for( int i = 0; i < foliageCoordsLength; i++ )
		{
			delete [] foliageCoords[i];
		}
		delete [] foliageCoords;
	}

	virtual void init(float heightInit, float widthInit, float foliageDensityInit){

		heightVariance = (std::max)(1, (int) (heightInit * baseHeightVariance));
		if (heightInit > 0.5) foliageHeight = 5;
		widthScale = widthInit * baseWidthScale;
		foliageDensity = foliageDensityInit * baseFoliageDensity;
	}
	virtual bool place(Level *level, Random *random, int x, int y, int z){

		thisLevel = level;
		int seed = random->nextLong();
		rnd->setSeed(seed);
		origin[0] = x;
		origin[1] = y;
		origin[2] = z;
		// Feature instances may be reused by sapling callers. Release the previous
		// canopy and choose a fresh height before checking the new location.
		for (int i = 0; i < foliageCoordsLength; ++i) delete [] foliageCoords[i];
		delete [] foliageCoords;
		foliageCoords = NULL;
		foliageCoordsLength = 0;
		height = minimumHeight + rnd->nextInt(heightVariance);
		if (y < 1 || y + height >= Level::DEPTH) return false;
		if (!(checkLocation()))
		{

			return false;
		}



		prepare();

		makeFoliage();

		makeTrunk();

		makeBranches();

		return true;
	}
};
#endif /*NET_MINECRAFT_WORLD_LEVEL_LEVELGEN_FEATURE__BasicTree_H__*/