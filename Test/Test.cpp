#include <stdio.h>


bool RemoteTest() {

	SimDisp_Stack SimDisp0;

	if (SimDisps_Create(&SimDisp0))
		return true;

	SimDisps_SetSize(&SimDisp0, 800, 480);
	SimDisps_SetTitle(&SimDisp0, "SimDisp");
	SimDisps_Resizeable(&SimDisp0, true);
	SimDisps_Open(&SimDisp0);
	//SimDisps_SetMouseActive(&SimDisp0, [](tSimDisp_MState, int32_t x, int32_t y, int32_t z) {
	//	printf("x: %3d, y: %3d\n", x, y);
	//});
	SimDisps_FillFull(&SimDisp0, 0xff);

	return false;

}

bool LocalTest() {

	if (SimDisp_InitDll("SimDisp.dll"))
		return true;

	SimDisp_SetSize(800, 480);
	SimDisp_SetTitle("SimDisp");
	SimDisp_Resizeable(true);
	SimDisp_Open();

	for (;;)
		SimDisp_FillFull(0xff);

}

int main(int argc, char *args) {

	_ConInit();

	LocalTest();

	return 0;

}
