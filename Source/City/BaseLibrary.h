// Fill out your copyright notice in the Description page of Project Settings.

#pragma once



#include "stdlib.h"
#include <queue>
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "City.h"
#include "Algo/Reverse.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include <functional>
#include "Runtime/Engine/Classes/Engine/TextRenderActor.h"
#include "BaseLibrary.generated.h"

/**
 * 
 */
struct SplitStruct {
	int min;
	int max;
	FVector p1;
	FVector p2;
};


static FRandomStream baseLibraryStream;

struct FPolygon;
struct FMaterialPolygon;

UENUM(BlueprintType)
enum class RoadType : uint8
{
	main 	UMETA(DisplayName = "Main Road"),
	secondary UMETA(DisplayName = "Secondary Road")
};

UENUM(BlueprintType)
enum class GenerationMode : uint8
{
	complete 	UMETA(DisplayName = "Complete generation"),
	procedural_aggressive UMETA(DisplayName = "Aggressive procedural generation"),
	procedural_relaxed UMETA(DisplayName = "Relaxed procedural generation")

};


UENUM(BlueprintType)
enum class RoomType : uint8
{
	office 	UMETA(DisplayName = "Office"),
	apartment UMETA(DisplayName = "Apartment"),
	store UMETA(DisplayName = "Store"),
	restaurant UMETA(DisplayName = "Restaurant")
};

UENUM(BlueprintType)
enum class PolygonType : uint8
{
	interior 	UMETA(DisplayName = "Interior"),
	exterior UMETA(DisplayName = "Exterior"),
	exteriorSnd UMETA(DisplayName = "Exterior Secondary"),
	floor UMETA(DisplayName = "Floor"),
	window UMETA(DisplayName = "Window"),
	windowFrame UMETA(DisplayName = "Window Frame"),
	occlusionWindow UMETA(DisplayName = "Occlusion Window"),
	roof UMETA(DisplayName = "Roof"),
	green UMETA(DisplayName = "Green"),
	concrete UMETA(DisplayName = "Concrete"),
	roadMiddle UMETA(DisplayName = "Middle of road line"),
	asphalt UMETA(DisplayName = "Road Material")
};

void getMinMax(float &min, float &max, FVector tangent, TArray<FVector> points);

FVector intersection(FPolygon &p1, TArray<FPolygon> &p2);
FVector intersection(FPolygon &p1, FPolygon &p2);
FVector intersection(FVector p1, FVector p2, FVector p3, FVector p4);
FVector intersection(FVector p1, FVector p2, FPolygon p);
bool selfIntersection(FPolygon &p1);
bool testCollision(FPolygon &, TArray<FPolygon> &, float leniency, FPolygon &);
bool testCollision(FPolygon &, FPolygon &, float leniency);
bool testCollision(TArray<FVector> tangents, TArray<FVector> vertices1, TArray<FVector> vertices2, float collisionLeniency);
FVector NearestPointOnLine(FVector linePnt, FVector lineDir, FVector pnt);
TArray<FMaterialPolygon> getSidesOfPolygon(FPolygon p, PolygonType type, float width);
TArray<FMaterialPolygon> fillOutPolygons(TArray<FMaterialPolygon> &first);
FVector intersection(FVector p1, FVector p2, FVector p3, FVector p4);
TArray<FPolygon> getBlockingEntrances(TArray<FVector> points, TSet<int32> entrances, TMap<int32, FVector> specificEntrances, float entranceWidth, float blockingLength);

FPolygon getEntranceHole(FVector p1, FVector p2, float floorHeight, float doorHeight, float doorWidth, FVector doorPos);

FPolygon getTinyPolygon(FVector point);

static FVector getNormal(FVector p1, FVector p2, bool left) {
	return FRotator(0, left ? 90 : 270, 0).RotateVector(p2 - p1);
}

FVector fitPolygonNextToPolygon(FPolygon &toFitAround, FPolygon &toMove, int place, FRotator offsetRot);

FPolygon getPolygon(FRotator rot, FVector pos, FString name, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map);


static FVector middle(FVector p1, FVector p2) {
	return (p2 + p1) / 2;
}

USTRUCT(BlueprintType)
struct FPolygon
{
	GENERATED_USTRUCT_BODY();

	FVector normal = FVector(0, 0, 0);

	UPROPERTY(BlueprintReadWrite)
		TArray<FVector> points;

	bool getIsClockwise() {
		float tot = 0;
		FVector first = points[0];
		for (int i = 1; i < points.Num() + 1; i++) {
			tot += (points[i-1].X*0.01 * points[i%points.Num()].Y*0.01 - points[i%points.Num()].X*0.01 * points[i-1].Y*0.01);
		}
			//UE_LOG(LogTemp, Warning, TEXT("getisclockwise res : %f"), tot);
		return tot > 0;
	}

	FVector& operator[] (int index) {
		return points[index];
	}

	void operator+= (FVector toAdd) {
		points.Add(toAdd);
	}

	FVector getCenter() {
		FVector center = FVector(0, 0, 0);
		double totLen = 0;
		for (int i = 1; i < points.Num() + 1; i++) {
			float len = (points[i%points.Num()] - points[i - 1]).Size();
			center += ((points[i%points.Num()] - points[i - 1]) / 2 + points[i - 1])*len;
			totLen += len;
		}
		center /= totLen;
		return center;
	}

	// not totally random, favors placement closer to the sides a bit, but good enough
	FVector getRandomPoint(bool left, float minDist, FRandomStream &stream = baseLibraryStream) {
		int place = stream.RandRange(1, points.Num());
		FVector tangent = (points[place%points.Num()] - points[place - 1]);
		FVector beginPlace = stream.FRand() * tangent + points[place - 1];
		tangent.Normalize();
		FVector pointNormal = FRotator(0, left ? 90 : 270, 0).RotateVector(tangent);
		int a;
		FVector target;
		getSplitCorrespondingPoint(place, beginPlace, pointNormal, a, target);
		if (target.X == 0.0f || FVector::Dist(beginPlace, target) < minDist * 2)
			return FVector(0, 0, 0);
		FVector point = stream.FRandRange(minDist, FVector::Dist(beginPlace, target) - minDist) * pointNormal + beginPlace;
		return point;
	}

	// only cares about dimensions X and Y, not Z
	double getArea() {
		double tot = 0;

		for (int i = 0; i < points.Num(); i++) {
			tot += 0.0001*(points[i].X * points[(i + 1) % points.Num()].Y);
			tot -= 0.0001*(points[i].Y * points[(i + 1) % points.Num()].X);
		}

		tot *= 0.5;
		return std::abs(tot);
	}

	void offset(FVector offset) {
		for (FVector &f : points) {
			f += offset;
		}
	};

	void rotate(FRotator rotation) {
		FVector center = getCenter();
		for (FVector &f : points) {
			f = rotation.RotateVector(f - center) + center;
		}
	}

	// removes corners that stick out in an ugly way
	void clipEdges(float maxDot) {
		bool changed = true;
		while (changed) {
			changed = decreaseEdges();
			for (int i = 1; i < points.Num(); i++) {
				FVector tan1 = points[i] - points[i - 1];
				FVector tan2 = points[(i + 1)%points.Num()] - points[i];
				tan1.Normalize();
				tan2.Normalize();
				float dist = FVector::DotProduct(tan1, tan2);
				if (dist < maxDot) {
					points.RemoveAt(i);
					i--;
					changed = true;
					break;
				}

			}
		}

		// untangle

		for (int i = 1; i < points.Num(); i++) {
			FVector tan1 = points[i] - points[i - 1];
			tan1.Normalize();
			for (int j = i + 2; j < points.Num() + 1; j++) {
				FVector tan2 = points[j%points.Num()] - points[j-1];
				tan2.Normalize();
				FVector res = intersection(points[i - 1], points[i], points[j - 1], points[j%points.Num()] - tan2*10);
				if (res.X != 0.0f) {
					TArray<FVector> newPoints;
					for (int k = 0; k < i; k++) {
						newPoints.Add(points[k]);
					}
					newPoints.Add(res);
					for (int k = j; k < points.Num(); k++) {
						newPoints.Add(points[k]);
					}
					points = newPoints;
					i = 0;
					break;

				}
			}
		}
	}





	// this method merges polygon sides when possible, and combines points
	bool decreaseEdges() {
		float distDiffAllowed = 40000;
		bool hasModified = false;
		for (int i = 1; i < points.Num() + 1; i++) {
			if (FVector::DistSquared(points[i - 1], points[i%points.Num()]) < distDiffAllowed) {
				points.RemoveAt(i%points.Num());
				hasModified = true;
				i--;
			}
		}

		return hasModified;
	}

	// assumes at least 3 points in polygon
	FVector getDirection() {
		FVector res = normal.Size() < 1.0f ? FVector::CrossProduct(points[1] - points[0], points[points.Num()-1] - points[0]) : normal;
		res.Normalize();
		return res;
	}

	void reverse() {
		Algo::Reverse(points);
	}

	void getSplitCorrespondingPoint(int begin, FVector point, FVector inNormal, int &split, FVector &p2) {
		float closest = 10000000.0f;
		for (int i = 1; i < points.Num()+1; i++) {
			if (i == begin) {
				continue;
			}
			FVector curr = intersection(point, point + inNormal * 100000, points[i - 1], points[i%points.Num()]);
			if (curr.X != 0.0f && FVector::Dist(curr, point) < closest) {
				closest = FVector::Dist(curr, point);
				split = i;
				p2 = curr;
			}
		}

	}

	SplitStruct getSplitProposal(bool buildLeft, float approxRatio, int preDeterminedNum = -1) {

		if (points.Num() < 3) {
			return SplitStruct{ 0, 0, FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f) };
		}
		int longest = -1;
		FVector curr;
		if (preDeterminedNum > -1) {
			longest = preDeterminedNum;
		}
		else {
			float longestLen = 0.0f;
			for (int i = 1; i < points.Num() + 1; i++) {
				float dist = FVector::DistSquared(points[i%points.Num()], points[i - 1]);
				if (dist > longestLen) {
					longestLen = dist;
					longest = i;
				}
			}
		}

		if (longest == -1) {
			return SplitStruct{ 0, 0, FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f) };
		}
		curr = points[longest%points.Num()] - points[longest - 1];
		curr.Normalize();

		FVector middle = (points[longest%points.Num()] - points[longest - 1]) * approxRatio + points[longest - 1];
		FVector p1 = middle;
		int split = 0;
		FVector p2 = FVector(0.0f, 0.0f, 0.0f);
		FVector tangent = FRotator(0, buildLeft ? 90 : 270, 0).RotateVector(curr);
		tangent.Normalize();

		getSplitCorrespondingPoint(longest, middle, tangent, split, p2);

		if (p2.X == 0.0f || p1.X == 0.0f) {
			UE_LOG(LogTemp, Warning, TEXT("UNABLE TO SPLIT, NO CORRESPONDING SPLIT POINT FOR POLYGON"));
			// cant split, no target, this shouldn't happen unless the polygons are poorly constructed
			return SplitStruct{ 0, 0, FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f) };

		}



		int min = longest;
		int max = split;

		// rearrange if split comes before longest in the array, they are expected to come out in order
		if (longest > split) {
			FVector temp = p1;
			p1 = p2;
			p2 = temp;
			min = split;
			max = longest;
		}

		return SplitStruct{ min, max, p1, p2 };
	}

	FVector getPointDirection(int place, bool left) {
		int prev = place == 0 ? points.Num() - 1 : place - 1;

		FVector dir1 = getNormal(points[place], points[prev], left);
		FVector dir2 = getNormal(points[(place + 1) % points.Num()], points[place] , left);

		dir1.Normalize();
		dir2.Normalize();

		FVector totDir = dir1 + dir2;// - FVector::DotProduct(dir1, dir2) * (dir1);
		totDir.Normalize();
		return totDir;

	}

	void symmetricShrink(float length, bool left) {
		for (int i = 0; i < points.Num(); i++) {
			points[i] += getPointDirection(i, left)*length;
		}
	}



	FVector getRoomDirection() {
		if (points.Num() < 3) {
			return FVector(0.0f, 0.0f, 0.0f);
		}
		return getNormal(points[1], points[0], true);
	}

};





USTRUCT(BlueprintType)
struct FMaterialPolygon : public FPolygon {
	GENERATED_USTRUCT_BODY();

	PolygonType type = PolygonType::exterior;
	float width = 20;
	bool overridePolygonSides = true;
};

USTRUCT(BlueprintType)
struct FMeshInfo {
	GENERATED_USTRUCT_BODY();

	FMeshInfo() :
		description(""),
		transform(FTransform()) {}
		FMeshInfo(const FString &description, const FTransform &transform) :
		description(description),
		transform(transform) {}


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FTransform transform;
};

UENUM(BlueprintType)
enum class WindowType : uint8
{
	rectangular 	UMETA(DisplayName = "Rectangular"),
	cross UMETA(DisplayName = "Cross"),
	rectangularHorizontalBigger UMETA(DisplayName = "Rectangular Horizontal Bigger"),
	verticalLines UMETA(DisplayName = "Vertical Lines")
};


USTRUCT(BlueprintType)
struct FRoomInfo {

	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FMaterialPolygon> pols;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FMeshInfo> meshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<UTextRenderComponent*> texts;

	void append(FRoomInfo info) {
		pols.Append(info.pols);
		meshes.Append(info.meshes);
	}

	void offset(FVector offset) {
		for (FPolygon &p : pols)
			p.offset(offset);
		for (FMeshInfo &f : meshes)
			f.transform.SetTranslation(f.transform.GetTranslation() + offset);
	}
};


UENUM(BlueprintType)
enum class SimplePlotType : uint8
{
	asphalt UMETA(DisplayName = "Asphalt"),
	green UMETA(DisplayName = "Green"),
	undecided UMETA(DisplayName = "Undecided")
};



TArray<FMeshInfo> placeRandomly(FPolygon pol, TArray<FPolygon> &blocking, int num, FString name, bool useRealPolygon = false, const TMap<FString, UHierarchicalInstancedStaticMeshComponent*> *map = nullptr);
TArray<FMeshInfo> attemptPlaceClusterAlongSide(FPolygon pol, TArray<FPolygon> &blocking, int num, float distBetween, FString name, FVector offset, bool useRealPolygon = false, const TMap<FString, UHierarchicalInstancedStaticMeshComponent*> *map = nullptr);
void attemptPlaceCenter(FPolygon &pol, TArray<FPolygon> &placed, TArray<FMeshInfo> &meshes, FString string, FRotator offsetRot, FVector offsetPos, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map);
void placeRows(FPolygon *r2, TArray<FPolygon> &placed, TArray<FMeshInfo> &meshes, FRotator offsetRot, FString name, float vertDens, float horDens, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map, bool left = false);
FMeshInfo getEntranceMesh(FVector p1, FVector p2, FVector doorPos);


USTRUCT(BlueprintType)
struct FSimplePlot {
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FPolygon pol;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FMeshInfo> meshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FPolygon> obstacles;

	SimplePlotType type = SimplePlotType::undecided;


	void decorate(TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map) {
		decorate(TArray<FPolygon>(), map);
	}


	void decorate(TArray<FPolygon> blocking, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map);

};

USTRUCT(BlueprintType)
struct FTextStruct {
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform pos;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString string;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString font;

};

USTRUCT(BlueprintType)
struct FHouseInfo {
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRoomInfo roomInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSimplePlot> remainingPlots;


};







static TArray<int32> getIntList(int32 min, int32 max) {
	TArray<int32> ints;
	for (int32 i = min; i < max; i++) {
		ints.Add(i);
	}
	return ints;
}


static void removeAllButOne(TSet<int32> &entries) {
	TArray<int> numbers;
	if (entries.Num() < 2) {
		return;
	}
	for (int32 i : entries) {
		numbers.Add(i);
	}
	int place = FMath::Rand() % numbers.Num();
	numbers.RemoveAt(place);
	for (int32 i : numbers) {
		entries.Remove(i);
	}
}





USTRUCT(BlueprintType)
struct FMetaPolygon : public FPolygon
{

	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool open = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool buildLeft;

	void checkOrientation() {
		if (!getIsClockwise())
			reverse();
		buildLeft = true;

	}



};

UENUM(BlueprintType)
enum class SubRoomType : uint8
{
	meeting UMETA(DisplayName = "Meeting Room"),
	work UMETA(DisplayName = "Work Room"),
	bath  UMETA(DisplayName = "Bathroom"),
	empty  UMETA(DisplayName = "Empty"),
	corridor UMETA(DisplayName = "Corridor"),
	bed UMETA(DisplayName = "Bedroom"),
	kitchen UMETA(DisplayName = "Kitchen"),
	living UMETA(DisplayName = "Living Room"),
	closet UMETA(DisplayName = "Closet"),
	hallway UMETA(DisplayName = "Hallway"),
	storeFront UMETA(DisplayName = "Store Front"),
	storeBack UMETA(DisplayName = "Store Back"),
	restaurant UMETA(DisplayName = "Restaurant")


};


// this tells us whether the room type is allowed to branch into several rooms, a bathroom should for example not lead into several other rooms, whereas a living room might
static bool splitableType(SubRoomType type) {
	switch (type){
	case SubRoomType::meeting: return false;
	case SubRoomType::work: return true;
	case SubRoomType::bath: return false;
	case SubRoomType::empty: return true;
	case SubRoomType::corridor: return true;
	case SubRoomType::bed: return false;
	case SubRoomType::kitchen: return true;
	case SubRoomType::living: return true;
	case SubRoomType::closet: return false;
	case SubRoomType::hallway: return true;
	case SubRoomType::storeBack: return true;
	case SubRoomType::storeFront: return true;
	case SubRoomType::restaurant: return true;

	}
	return true;
}

static bool isOnLine(FVector point, FVector p1, FVector p2) {
	return std::abs(FVector::Dist(point, p1) + FVector::Dist(point, p2) - FVector::Dist(p1, p2)) < 1.0f;
}


struct RoomSpecification {
	float minArea;
	float maxArea;
	SubRoomType type;
};

struct RoomBlueprint {
	RoomBlueprint(const TArray<RoomSpecification> &needed, const TArray<RoomSpecification> &optional) : needed(needed), optional(optional), useHallway(false) {}
	RoomBlueprint(const TArray<RoomSpecification> &needed, const TArray<RoomSpecification> &optional, bool useHallway) : needed(needed), optional(optional), useHallway(useHallway) {}

	TArray<RoomSpecification> needed;
	TArray<RoomSpecification> optional;
	bool useHallway;

};





struct FRoomPolygon : public FPolygon
{
	// sides of the polygon where windows are allowed
	TSet<int32> windows;
	// sides of the polygon with entrances from my end
	TSet<int32> entrances;
	// sides of polygons that are face the outside of the building
	TSet<int32> exteriorWalls;
	// a subset of entrances where the entrance is not neccessarily in the middle but in a specified position
	TMap<int32, FVector> specificEntrances;
	// sides that should not be rendered when building the room
	TSet<int32> toIgnore;
	// a subset of toIgnore that have entrances from neighboring rooms
	TMap<int32, TSet<FRoomPolygon*>> passiveConnections;
	// a subset of entrances where the have a side which I have an entrance towards
	TMap<FRoomPolygon*, int32> activeConnections;

	WindowType windowType;

	bool canRefine = true;
	SubRoomType type = SubRoomType::empty;

	void updateConnections(int num, FVector &inPoint, FRoomPolygon* newP, bool first, int passiveNum, bool preferEntrancesInThis) {
		TArray<FRoomPolygon*> toRemove;

		TSet<FRoomPolygon*> childPassive;

		// move collisions to the left if preferEntrancesInThis
		if (specificEntrances.Contains(num)) {
			//FVector entrancePoint = specificEntrances.Contains(num) ? specificEntrances[num] : middle(points[num], points[num - 1]);
			if (FVector::Dist(inPoint, specificEntrances[num]) < 100) {
				FVector tangent = points[num%points.Num()] - points[num - 1];
				tangent.Normalize();
				inPoint = preferEntrancesInThis ^ !first ? specificEntrances[num] - tangent * 100 : specificEntrances[num] + tangent * 100;
				//float toMove = FVector::Dist(inPoint, specificEntrances[num] - tangent * 100);
				//inPoint -= toMove * tangent;
			}
		}

		if (!passiveConnections.Contains(num)) {
			return;
		}
		for (FRoomPolygon* p1 : passiveConnections[num]) {
			int loc = p1->activeConnections[this];
			// this is the entrance, we're not allowed to place a wall that collides with this, try to fit newP on the other side of the entrance if possible, making it a single-entry room
			FVector point = p1->specificEntrances[loc];
			float dist = FVector::Dist(point, inPoint);
			if (dist < 100) {
				FVector tangent = points[num%points.Num()] - points[num - 1];
				tangent.Normalize();
				inPoint = preferEntrancesInThis^!first ? point - tangent * 100 : point + tangent * 100;
				//float toMove = FVector::Dist(inPoint, point - tangent * 100);
				//inPoint -= toMove * tangent;
				// our child is now free from our master, but we're still slaves
			}
			//else {
				// new cut doesn't interfere overlap with the entrance, now to see who gets it
				if (first && isOnLine(point, points[num - 1], inPoint) || !first && !isOnLine(point, points[num - 1], inPoint)) {
					// i got it, no change
				}
				else {
					// my child got it
					childPassive.Add(p1);
					p1->activeConnections.Remove(this);
					p1->activeConnections.Add(newP, loc);
					toRemove.Add(p1);
					newP->toIgnore.Add(passiveNum);
				}
			//}
		}
		if (childPassive.Num() > 0)
			newP->passiveConnections.Add(passiveNum, childPassive);

		for (FRoomPolygon* t : toRemove) {
			passiveConnections[num].Remove(t);
		}
	}
	FRoomPolygon* splitAlongSplitStruct(SplitStruct p, bool entranceBetween) {

		// determine if any room has no entrances yet, so we can try to cluster all entrances in the other room
		int prelEntrancesNewP = 0;
		for (int i = p.min + 1; i < p.max - 1; i++) {
			if (entrances.Contains(i))
				prelEntrancesNewP++;
		}
		bool clusterDoorsInThis = prelEntrancesNewP == 0;

		FRoomPolygon* newP = new FRoomPolygon();
		updateConnections(p.min, p.p1, newP, true, 1, clusterDoorsInThis);

		if (entrances.Contains(p.min)){
				// potentially add responsibility of child
				FVector entrancePoint = specificEntrances.Contains(p.min) ? specificEntrances[p.min] : middle(p.p1, points[p.min - 1]);
				if (isOnLine(entrancePoint, p.p1, points[p.min])) {
					if (specificEntrances.Contains(p.min))
						newP->specificEntrances.Add(1, entrancePoint);
					newP->entrances.Add(1);
					TArray<FRoomPolygon*> toRemove;
					for (auto &pair : activeConnections) {
						if (pair.Value == p.min) {
							for (auto &pair2 : pair.Key->passiveConnections) {
								if (pair2.Value.Contains(this)) {
									pair.Key->passiveConnections[pair2.Key].Add(newP);
									pair.Key->passiveConnections[pair2.Key].Remove(this);
									break;
								}
							}
							newP->activeConnections.Add(pair.Key, 1);
							toRemove.Add(pair.Key);
							break;
						}
					}
					specificEntrances.Remove(p.min);
					entrances.Remove(p.min);
					for (FRoomPolygon* p2 : toRemove) {
						activeConnections.Remove(p2);
					}
			}
		}


		if (windows.Contains(p.min)) {
			newP->windows.Add(1);
		}
		if (exteriorWalls.Contains(p.min)) {
			newP->exteriorWalls.Add(1);
		}

		if (toIgnore.Contains(p.min)) {
			newP->toIgnore.Add(1);
		}
		newP->points.Add(p.p1);

		newP->points.Add(points[p.min]);

		for (int i = p.min + 1; i < p.max; i++) {
			if (entrances.Contains(i)) {

				if (specificEntrances.Contains(i))
					newP->specificEntrances.Add(newP->points.Num(), specificEntrances[i]);
				newP->entrances.Add(newP->points.Num());

				entrances.Remove(i);
				specificEntrances.Remove(i);
				TArray<FRoomPolygon*> toRemove;
				for (auto &pair : activeConnections) {
					if (pair.Value == i) {
						FRoomPolygon *p1 = pair.Key;
						for (auto &a : p1->passiveConnections) {
							if (a.Value.Contains(this)) {
								toRemove.Add(p1);
								//activeConnections.Remove(p1);
								a.Value.Remove(this);
								a.Value.Add(newP);
								newP->activeConnections.Add(p1, newP->points.Num());
								break;
							}
						}
					}
				}
				for (FRoomPolygon *f : toRemove) {
					activeConnections.Remove(f);
				}
			}
			if (windows.Contains(i)) {
				windows.Remove(i);
				newP->windows.Add(newP->points.Num());
			}
			if (exteriorWalls.Contains(i)) {
				exteriorWalls.Remove(i);
				newP->exteriorWalls.Add(newP->points.Num());
			}
			if (toIgnore.Contains(i)) {
				toIgnore.Remove(i);
				newP->toIgnore.Add(newP->points.Num());
				if (passiveConnections.Contains(i)) {
					TSet<FRoomPolygon*> pols = passiveConnections[i];
					newP->passiveConnections.Add(newP->points.Num(), pols);
					passiveConnections.Remove(i);
					for (FRoomPolygon *p1 : pols) {
						p1->activeConnections.Add(newP, p1->activeConnections[this]);
						p1->activeConnections.Remove(this);
					}
				}
			}
			newP->points.Add(points[i]);
		}

		updateConnections(p.max, p.p2, newP, false, newP->points.Num(), clusterDoorsInThis);


		if (entrances.Contains(p.max)){

				FVector entrancePoint = specificEntrances.Contains(p.max) ? specificEntrances[p.max] : middle(p.p2, points[p.max%points.Num()]);
				if (isOnLine(entrancePoint, p.p2, points[p.max - 1])) {
					if (specificEntrances.Contains(p.max))
						newP->specificEntrances.Add(newP->points.Num(), entrancePoint);

					newP->entrances.Add(newP->points.Num());
					TArray<FRoomPolygon*> toRemove;
					for (auto &pair : activeConnections) {
						if (pair.Value == p.max) {
							for (auto &pair2 : pair.Key->passiveConnections) {
								if (pair2.Value.Contains(this)) {
									pair.Key->passiveConnections[pair2.Key].Add(newP);
									pair.Key->passiveConnections[pair2.Key].Remove(this);
									break;
								}
							}
							newP->activeConnections.Add(pair.Key, newP->points.Num());
							toRemove.Add(pair.Key);
							break;
						}
					}
					specificEntrances.Remove(p.max);
					entrances.Remove(p.max);
					for (FRoomPolygon* p2 : toRemove) {
						activeConnections.Remove(p2);
					}
				}
		}
		if (windows.Contains(p.max)) {
			newP->windows.Add(newP->points.Num());
		}
		if (exteriorWalls.Contains(p.max)) {
			newP->exteriorWalls.Add(newP->points.Num());
		}


		if (toIgnore.Contains(p.max)) {
			newP->toIgnore.Add(newP->points.Num());
		}



		TSet<int32> newList;
		TMap<int32, FVector> newSpecificList;
		for (int32 i : entrances) {
			if (i >= p.max){
				newList.Add(i - (p.max - p.min) + 2);
				if (specificEntrances.Contains(i))
					newSpecificList.Add(i - (p.max - p.min) + 2, specificEntrances[i]);
			}
			else {
				newList.Add(i);
				if (specificEntrances.Contains(i))
					newSpecificList.Add(i, specificEntrances[i]);
			}
		}
		entrances = newList;
		specificEntrances = newSpecificList;

		newList.Empty();
		for (int32 i : windows) {
			if (i >= p.max)
				newList.Add(i - (p.max - p.min) + 2);
			else
				newList.Add(i);
		}
		windows = newList;

		newList.Empty();
		for (int32 i : exteriorWalls) {
			if (i >= p.max)
				newList.Add(i - (p.max - p.min) + 2);
			else
				newList.Add(i);
		}
		exteriorWalls = newList;

		newList.Empty();
		for (int32 i : toIgnore) {
			if (i >= p.max)
				newList.Add(i - (p.max - p.min) + 2);
			else
				newList.Add(i);
		}
		toIgnore = newList;



		TMap<FRoomPolygon*, int32> newActive;
		for (auto &pair : activeConnections) {
			if (pair.Value >= p.max) {
				newActive.Add(pair.Key, pair.Value - (p.max - p.min) + 2);
			}
			else {
				newActive.Add(pair.Key, pair.Value);
			}
		}
		activeConnections = newActive;

		TMap<int32, TSet<FRoomPolygon*> > newPassive;
		for (auto &pair : passiveConnections) {
			if (pair.Key >= p.max) {
				newPassive.Add(pair.Key - (p.max - p.min) + 2, pair.Value );
			}
			else {
				newPassive.Add(pair.Key, pair.Value);
			}
		}
		passiveConnections = newPassive;

		newP->points.Add(p.p2);

		//// dont place the wall twice
		toIgnore.Add(p.min+1);
		//	// entrance to next room
		if (entranceBetween){
			TSet<FRoomPolygon*> passive = newP->passiveConnections.Contains(newP->points.Num()) ? newP->passiveConnections[newP->points.Num()] : TSet<FRoomPolygon*>();
			passive.Add(newP);
			passiveConnections.Add(p.min + 1, passive);
			newP->specificEntrances.Add(newP->points.Num(), middle(p.p1, p.p2));
			newP->entrances.Add(newP->points.Num());
			newP->activeConnections.Add(this, newP->points.Num());
		}


		points.RemoveAt(p.min, p.max - p.min);
		points.EmplaceAt(p.min, p.p1);
		points.EmplaceAt(p.min + 1, p.p2);

		int entrancesThis = getTotalConnections();
		int entrancesNewP = newP->getTotalConnections();



		return newP;
	}

	FTransform attemptGetPosition(TArray<FPolygon> &placed, TArray<FMeshInfo> &meshes, bool windowAllowed, int testsPerSide, FString string, FRotator offsetRot, FVector offsetPos, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map, bool onWall);
	bool attemptPlace(TArray<FPolygon> &placed, TArray<FMeshInfo> &meshes, bool windowAllowed, int testsPerSide, FString string, FRotator offsetRot, FVector offsetPos, TMap<FString, UHierarchicalInstancedStaticMeshComponent*> map, bool onWall);


	TArray<FRoomPolygon*> fitSpecificationOnRooms(TArray<RoomSpecification> specs, TArray<FRoomPolygon*> &remaining, bool repeating, bool useMin) {
		TArray<FRoomPolygon*> toReturn;
		float minPctSplit = 0.35f;

		bool couldPlace = false;
		bool anyRoomPlaced = false;
		int count = 0;
		do {
			anyRoomPlaced = false;
			for (RoomSpecification spec : specs) {
				if (remaining.Num() == 0)
					return toReturn;
				couldPlace = false;
				bool specSmallerThanRooms = true;
				float maxAreaAllowed = useMin ? (spec.maxArea + spec.minArea) / 2 : spec.maxArea;
				for (int i = 0; i < remaining.Num(); i++) {
					FRoomPolygon *r = remaining[i];
					float area = r->getArea();
					specSmallerThanRooms = specSmallerThanRooms && area > maxAreaAllowed;
					if (area < maxAreaAllowed && area > spec.minArea) {
						// can place here
						r->type = spec.type;
						toReturn.Add(r);
						remaining.RemoveAt(i);
						couldPlace = true;
						anyRoomPlaced = true;
						break;
					}
				}
				if (!couldPlace && specSmallerThanRooms) {
					// could not find a fitting room since all remaining are too big, cut them down to size
					FRoomPolygon *target = remaining[0];
					int targetNum = 0;
					float scale = 0.0f;
					for (int i = 0; i < remaining.Num(); i++) {
						target = remaining[i];
						targetNum = i;
						scale = spec.minArea / target->getArea();
						if (scale < 1.0f) {
							//current target is bigger than my requirement
							break;
						}
					}
					remaining.RemoveAt(targetNum);
					scale = maxAreaAllowed / target->getArea();
					// keep cutting it down until small enough to place my room
					bool canPlace = true;
					int count2 = 0;
					while (scale < 1.0f && ++count2 < 5) {
						FRoomPolygon* newP = target->splitAlongMax(0.5, true);
						if (newP == nullptr) {
							remaining.Add(target);
							canPlace = false;
							break;
						}
						if (!splitableType(spec.type) && newP->getTotalConnections() < target->getTotalConnections() || splitableType(spec.type) && newP->getTotalConnections() > target->getTotalConnections()) {
							// swap rooms if newP is more suited for the purpose of the new room
							std::swap(newP, target);
						}
						remaining.EmplaceAt(0, newP);
						scale = maxAreaAllowed / target->getArea();
					}
					if (canPlace) {
						target->type = spec.type;
						toReturn.Add(target);
						anyRoomPlaced = true;
						couldPlace = true;
					}

				}
			}
		} while (repeating && anyRoomPlaced && ++count < 5);

		return toReturn;


	}

	int getTotalConnections() {
		int totPassive = 0;
		for (auto &list : passiveConnections) {
			totPassive += list.Value.Num();
		}
		return entrances.Num()	+ totPassive;
	}


	static SplitStruct getSealOffLine(FRoomPolygon *r2) {
		// get split struct for cut off room inside current room

		int current = 0;
		
		FVector firstB = FVector(0,0,0);
		FVector firstE = FVector(0,0,0);
		int firstI = -1;
		FVector currB = FVector(0,0,0);
		FVector currE = FVector(0,0,0);
		int currI = -1;
		float biggestArea = 10;
		FVector bestP1;
		FVector bestP2;
		int bestMin = -1;
		int bestMax = -1;

		for (int i = 1; i < r2->points.Num() + 1; i++) {
			FVector point = FVector(0, 0, 0);
			FVector tan = FVector(0, 0, 0);
			if (r2->entrances.Contains(i)) {
				point = r2->specificEntrances.Contains(i) ? r2->specificEntrances[i] : middle(r2->points[i%r2->points.Num()], r2->points[i - 1]);
				tan = r2->points[i%r2->points.Num()] - r2->points[i - 1];
				tan.Normalize();
			}
			else if (r2->passiveConnections.Contains(i)) {
				for (auto room : r2->passiveConnections[i]) {
					int loc = room->activeConnections[r2];
					point = room->specificEntrances[loc];
					tan = -room->points[loc%room->points.Num()] - room->points[loc - 1];
					tan.Normalize();
				}
			}
			if (point.X != 0.0f) {
				FVector newCurrB = point - tan * 101;
				FVector newCurrE = point + tan * 101;
				if (firstB.X == 0.0f) {
					firstB = newCurrB;
					firstE = newCurrE;
					firstI = i;

				}
				else {
					// try building polygon with newCurr and curr as split
					FPolygon temp;
					//temp += newCurrB;
					temp += currE;
					for (int j = currI; j < i; j++)
						temp += r2->points[j];
					temp += newCurrB;

					if (temp.getArea() > biggestArea) {
						bestP1 = currE;
						bestP2 = newCurrB;
						bestMin = currI;
						bestMax = i;
						biggestArea = temp.getArea();
					}
					//;temp
				}

				currE = newCurrE;
				currB = newCurrB;
				currI = i;

			}
		}

		FPolygon temp;
		//temp += newCurrB;
		temp += firstE;
		for (int j = firstI; j < currI; j++)
			temp += r2->points[j];
		temp += currB;

		if (temp.getArea() > biggestArea) {
			bestP1 = firstE;
			bestP2 = currB;
			bestMin = firstI;
			bestMax = currI;
			biggestArea = temp.getArea();
		}


		return SplitStruct{ bestMin, bestMax , bestP1, bestP2};


		//FVector p1Min = FVector(0, 0, 0);
		//FVector p1Max = FVector(0, 0, 0);
		//FVector p2Min = FVector(0, 0, 0);
		//FVector p2Max = FVector(0, 0, 0);
		//int i1 = 0;
		//int i2 = 0;
		//auto it = r2->entrances.CreateIterator();
		//while (it && p2Min.X == 0.0f) {
		//	FVector pos = r2->specificEntrances.Contains(*it) ? r2->specificEntrances[*it] : middle(r2->points[*it%r2->points.Num()], r2->points[*it - 1]);
		//	FVector tan = r2->points[*it%r2->points.Num()] - r2->points[*it - 1];
		//	tan.Normalize();
		//	FVector beginPos = pos - tan * 100;
		//	FVector endPos = pos + tan * 100;
		//	if (p1Min.X == 0.0f) {
		//		p1Min = beginPos;
		//		p1Max = endPos;
		//		i1 = *it;
		//	}
		//	else {
		//		p2Min = beginPos;
		//		p2Max = endPos;
		//		i2 = *it;
		//	}
		//	++it;
		//}
		//if (p2Min.X == 0.0f) {
		//	auto it2 = r2->passiveConnections.CreateIterator();
		//	while (it2 && p2Min.X == 0.0f) {
		//		FVector beginPos;
		//		FVector endPos;
		//		for (auto &room : it2->Value) {
		//			for (auto &passiveConn : room->passiveConnections) {
		//				if (passiveConn.Value.Contains(r2)) {
		//					int pos = room->activeConnections[r2];
		//					FVector tan = room->points[pos%room->points.Num()] - room->points[pos - 1];
		//					tan.Normalize();
		//					beginPos = room->specificEntrances[pos] - tan * 100;
		//					endPos = room->specificEntrances[pos] + tan * 100;
		//				}
		//			}
		//		}
		//		if (p1Min.X == 0.0f) {
		//			i1 = it2->Key;
		//			p1Min = beginPos;
		//			p1Max = endPos;

		//		}
		//		else {
		//			i2 = it2->Key;
		//			p2Min = beginPos;
		//			p2Max = endPos;
		//		}
		//		++it2;
		//	}
		//}
		//FVector p1 = p1Max;
		//FVector p2 = p2Min;
		//if (i1 > i2) {
		//	std::swap(i1, i2);
		//	p1 = p2Max;
		//	p2 = p1Min;
		//}
		//if (p1.X == 0.0f || p2.X == 0.0f)
		//	return SplitStruct{ -1, -1, FVector(0,0,0), FVector(0, 0, 0) };
		//return SplitStruct{i1,i2, p1, p2 };
	}



	FRoomPolygon* splitAndCreateSingleEntranceRoom(FRoomPolygon* room) {
		FRoomPolygon cp = *room;
		SplitStruct res = getSealOffLine(room);
		if (res.min == -1)
			return nullptr;
		FRoomPolygon *other = room->splitAlongSplitStruct(res, true);
		if (other == nullptr)
			return nullptr;
		if (other->getTotalConnections() > 1) {
			room->type = room->type;
			other->type = SubRoomType::corridor;
		}
		else {
			other->type = room->type;
			room->type = SubRoomType::corridor;
		}
		//	std::swap(other, room);

		return other;
		//for (int i = 1; i < room->points.Num(); i++) {
		//	FRoomPolygon *newP = room->splitAlongMax(0.5, true, i);

		//	if (newP != NULL) {
		//		if (newP->getTotalConnections() < 2) {
		//			newP->type = room->type;
		//			room->type = SubRoomType::corridor;
		//			return newP;
		//		}
		//		else {
		//			delete newP;
		//			*room = cp;
		//		}
		//	}
		//}
		//return nullptr;
	}

	void postFit(TArray<FRoomPolygon*> &rooms){

		TArray<FRoomPolygon*> toAdd;
		for (FRoomPolygon *room : rooms) {
			if (!splitableType(room->type) && room->getTotalConnections() > 1) {
				// need to modify room
				FRoomPolygon* res = splitAndCreateSingleEntranceRoom(room);
				if (res != nullptr)
					toAdd.Add(res);
			}
		}
		rooms.Append(toAdd);
		//TArray<RoomSpecification> neededRooms = blueprint.needed;
		//TMap<SubRoomType, int32> need;
		//for (RoomSpecification r : neededRooms) {
		//	if (need.Contains(r.type))
		//		need[r.type] ++;
		//	else
		//		need.Add(r.type, 1);
		//}
		//for (FRoomPolygon *p : rooms) {
		//	if (p->entrances.Num() > p->specificEntrances.Num() && blueprint.useHallway) {
		//		p->type = SubRoomType::hallway;
		//	}
		//	if (!splitableType(p->type) && p->getTotalConnections() > 1) {
		//		p->type = SubRoomType::corridor;
		//	}
		//	if (need.Contains(p->type))
		//		need[p->type] --;
		//}

		//TArray<SubRoomType> remaining;
		//for (auto &spec : need) {
		//	for (int i = 0; i < spec.Value; i++) {
		//		remaining.Add(spec.Key);
		//	}
		//}

		//while (remaining.Num() > 0) {
		//	SubRoomType current = remaining[remaining.Num() - 1];

		//	bool found = false;
		//	for (FRoomPolygon *p : rooms) {
		//		// first pick non-needed rooms
		//		if (!need.Contains(p->type) && ((splitableType(current) || p->getTotalConnections() < 2))) {
		//			p->type = current;
		//			found = true;
		//			break;
		//		}
		//	}
		//	if (found) {
		//		remaining.RemoveAt(remaining.Num() - 1);
		//		need[current] --;
		//		continue;
		//	}
		//	else {
		//		if (!splitableType(current)) {
		//			// consider picking rooms that already contains an essential room
		//			for (FRoomPolygon *p : rooms) {
		//				if (splitableType(p->type) && p->getTotalConnections() < 2) {
		//					SubRoomType prevType = p->type;
		//					remaining.RemoveAt(remaining.Num() - 1);
		//					need[current]--;
		//					if (++need[prevType] > 0) {
		//						remaining.Add(prevType);
		//					}
		//					p->type = current;
		//					found = true;
		//					break;
		//				}
		//			}

		//			for (FRoomPolygon *p : rooms) {
		//				if (p->getTotalConnections() == 2 && p->getArea() > 100) {
		//					SplitStruct res = getSealOffLine(p);
		//					if (res.p1.X == 0.0f)
		//						continue;
		//					FRoomPolygon *newP = p->splitAlongSplitStruct(res, true);
		//					newP->type = current;
		//					//SubRoomType prevType = p->type;
		//					need[current]--;
		//					remaining.RemoveAt(remaining.Num() - 1);

		//					//if (++need[prevType] > 0) {
		//					//	remaining.Add(prevType);
		//					//}
		//					//p->type = current;
		//					found = true;
		//					rooms.Add(newP);
		//					break;
		//				}
		//			}
		//			// if this fails, there are no possible places for this room, TODO create a new room somewhere
		//			if (!found) {
		//				need[current] --;
		//				remaining.RemoveAt(remaining.Num() - 1);
		//			}

		//		}
		//		else {
		//			// if this fails, there are no possible places for this room, TODO create a new room somewhere
		//			need[current] --;
		//			remaining.RemoveAt(remaining.Num() - 1);
		//		}

		//	}
		//}

	}

	FRoomPolygon* splitAlongMax(float approxRatio, bool entranceBetween, int preDeterminedNum = -1) {
		SplitStruct p = getSplitProposal(false, approxRatio, preDeterminedNum);
		if (p.p1.X == 0.0f) {
			return nullptr;
		}
		return splitAlongSplitStruct(p, entranceBetween);
	}

	TArray<FRoomPolygon*> getRooms(RoomBlueprint blueprint) {
		TArray<FRoomPolygon*> rooms;
		TArray<FRoomPolygon*> remaining;
		FRoomPolygon* thisP = new FRoomPolygon();
		*thisP = *this;
		if (blueprint.useHallway)
			thisP->type = SubRoomType::hallway;
		remaining.Add(thisP);
		removeAllButOne(remaining[0]->entrances);

		float standardAreaRequired = 0;
		// if the sum of the average room areas for neccesary rooms is greater than the total area of the apartment, use rooms as small as possible
		for (RoomSpecification r : blueprint.needed) {
			standardAreaRequired += (r.maxArea + r.minArea) / 2;
		}
		bool minimizeRoomSizes = standardAreaRequired > getArea();
		rooms.Append(fitSpecificationOnRooms(blueprint.needed, remaining, false, minimizeRoomSizes));
		rooms.Append(fitSpecificationOnRooms(blueprint.optional, remaining, true, false));

		rooms.Append(remaining);
		// fix unsplitable rooms

		//postFit(rooms);


		return rooms;
	}

};






USTRUCT(BlueprintType)
struct FHousePolygon : public FMetaPolygon {

	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector housePosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float height;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float population;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	RoomType type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	SimplePlotType simplePlotType;

	bool canBeModified = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TSet<int32> entrances;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TSet<int32> windows;

	void removePoint(int place) {
		if (points.Num() <= place) {
			return;
		}
		std::vector<int> toRemove;
		for (int i : windows) {
			if (i > place) {
				toRemove.push_back(i);
			}
		}
		for (int i : toRemove) {
			windows.Remove(i);
			windows.Add(i - 1);
		}
		toRemove.clear();
		for (int i : entrances) {
			if (i > place) {
				toRemove.push_back(i);
			}
		}
		for (int i : toRemove) {
			entrances.Remove(i);
			entrances.Add(i - 1);
		}
		points.RemoveAt(place);
	}
	void addPoint(int place, FVector point) {
		std::vector<int> toRemove;
		for (int i : windows) {
			if (i >= place) {
				toRemove.push_back(i);
			}
		}
		for (int i : toRemove) {
			windows.Remove(i);
			windows.Add(i + 1);
		}
		toRemove.clear();
		for (int i : entrances) {
			if (i >= place) {
				toRemove.push_back(i);
			}
		}
		for (int i : toRemove) {
			entrances.Remove(i);
			entrances.Add(i + 1);
		}
		points.EmplaceAt(place, point);
	}

	FHousePolygon splitAlongMax(float spaceBetween) {

		SplitStruct p = getSplitProposal(true, 0.5);
		if (p.p1.X == 0.0f) {
			return FHousePolygon();
		}

		FHousePolygon newP;
		newP.open = open;
		newP.buildLeft = buildLeft;
		newP.population = population;
		newP.type = type;

		FVector offset = p.p2 - p.p1;
		offset = FRotator(0, 90, 0).RotateVector(offset);
		offset.Normalize();

		newP.points.Add(p.p1);
		if (entrances.Contains(p.min)) {
			newP.entrances.Add(newP.points.Num());
		}
		if (windows.Contains(p.min)) {
			newP.windows.Add(newP.points.Num());
		}
		newP.points.Add(points[p.min]);

		for (int i = p.min + 1; i < p.max; i++) {
			if (entrances.Contains(i)) {
				entrances.Remove(i);
				newP.entrances.Add(newP.points.Num());
			}
			if (windows.Contains(i)) {
				windows.Remove(i);
				newP.windows.Add(newP.points.Num());
			}
			newP.points.Add(points[i]);
		}

		if (entrances.Contains(p.max)) {
			newP.entrances.Add(newP.points.Num());
		}
		if (windows.Contains(p.max)) {
			newP.windows.Add(newP.points.Num());
		}

		std::vector<int32> toRemove;
		for (int32 i : entrances) {
			if (i > p.min)
				toRemove.push_back(i);
		}
		for (int32 i : toRemove) {
			entrances.Remove(i);
			entrances.Add(i - (p.max - p.min) + 2);
		}


		toRemove.clear();
		for (int32 i : windows) {
			if (i > p.min)
				toRemove.push_back(i);
		}
		for (int32 i : toRemove) {
			windows.Remove(i);
			windows.Add(i - (p.max - p.min) + 2);
		}


		newP.points.Add(p.p2);
		//newP.points.Add(p.p1);
		
		points.RemoveAt(p.min, p.max - p.min);
		points.EmplaceAt(p.min, p.p1);
		points.EmplaceAt(p.min + 1, p.p2);

		//newP.checkOrientation();
		return newP;

	}

	TArray<FHousePolygon> recursiveSplit(float maxArea, float minArea, int depth, float spaceBetween) {

		double area = getArea();

		TArray<FHousePolygon> tot;

		if (points.Num() < 3 || area <= minArea) {
			return tot;
		}
		else if (depth > 2) {
			tot.Add(*this);
			return tot;
		}
		else if (area > maxArea) {
			FHousePolygon newP = splitAlongMax(spaceBetween);
			if (newP.points.Num() > 2) {
				tot = newP.recursiveSplit(maxArea, minArea, depth + 1, spaceBetween);
			}
			tot.Append(recursiveSplit(maxArea, minArea, depth + 1, spaceBetween));

		}
		else {
			tot.Add(*this);
		}
		return tot;
	}

	TArray<FHousePolygon> refine(float maxArea, float minArea, float spaceBetween) {

		decreaseEdges();
		TArray<FHousePolygon> toReturn;
		if (!open) {
			toReturn.Append(recursiveSplit(maxArea, minArea, 0, spaceBetween));
		}
		else 
			toReturn.Add(*this);
		return toReturn;
	}
};


USTRUCT(BlueprintType)
struct FLine {
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector p1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector p2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float width;

	FVector getMiddle() {
		return (p1 + p2) / 2;//(p2 - p1) / 2 + p1;
	}
};

USTRUCT(BlueprintType)
struct FRoadSegment : public FLine
{
	//GENERATED_BODY();

	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector beginTangent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector endTangent = FVector(0.0f, 0.0f, 0.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		RoadType type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector v1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector v2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector v3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector v4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool roadInFront;

};



struct logicRoadSegment {
	float time;
	logicRoadSegment* previous;
	FRoadSegment* segment;
	FRotator firstDegreeRot;
	FRotator secondDegreeRot;
	int roadLength;
};

struct roadComparator {
	bool operator() (logicRoadSegment* arg1, logicRoadSegment* arg2) {
		return arg1->time > arg2->time;
	}
};

struct roomComparator {
	bool operator() (FRoomPolygon *p1, FRoomPolygon *p2) {
		return p1->getArea() > p2->getArea();
	}
};

USTRUCT(BlueprintType)
struct FCityDecoration {
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FMaterialPolygon> polygons;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FMeshInfo> meshes;

};

USTRUCT(BlueprintType)
struct FPlotInfo {
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FHousePolygon> houses;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSimplePlot> leftovers;
};

USTRUCT(BlueprintType)
struct FPlotPolygon : public FMetaPolygon{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float population;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	RoomType type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	SimplePlotType simplePlotType;

};


struct Point {
	float x;
	float y;
};

class CITY_API BaseLibrary
{
public:
	BaseLibrary();
	~BaseLibrary();

	UFUNCTION(BlueprintCallable, Category = conversion)
	static TArray<FMaterialPolygon> getSimplePlotPolygons(TArray<FSimplePlot> plots);
	static TArray<FMetaPolygon> getSurroundingPolygons(TArray<FRoadSegment> &segments, TArray<FRoadSegment> &blocking, float stdWidth, float extraLen, float extraRoadLen, float width, float middleOffset);


};