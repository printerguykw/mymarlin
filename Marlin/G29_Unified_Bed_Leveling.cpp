#include "Marlin.h"

#if ENABLED(UNIFIED_BED_LEVELING_FEATURE)
#include "vector_3.h"
#include "qr_solve.h"

#include "Bed_Leveling.h"
#include "configuration_store.h"
#include "G29_Unified_Bed_Leveling.h"
#include "planner.h"


#include <avr/io.h>

void menu_action_setting_edit_float43(const char* pstr, float* ptr, float minValue, float maxValue);
void lcd_babystep_z();
void lcd_return_to_status();
bool lcd_clicked();
extern long babysteps_done;
void lcd_implementation_clear();
void lcd_mesh_edit_setup(float inital);
float lcd_mesh_edit();
void lcd_z_offset_edit_setup( float );
float lcd_z_offset_edit();
extern float meshedit_done;

/*
   G29: Unified Bed Leveling by Roxy
*/


// Transform required to compensate for bed level
//extern matrix_3x3 plan_bed_level_matrix;

/**
   Get the position applying the bed level matrix
*/

//vector_3 plan_get_position();

// static void set_bed_level_equation_lsq(double* plane_equation_coefficients);
// static void set_bed_level_equation_3pts(float z_at_pt_1, float z_at_pt_2, float z_at_pt_3);

/**
     G29: Delta Mesh Based Compensation System

     Parameters understood by this leveling system:

      A     Activate  Activate the Unified Bed Leveling system.

      B #   Business  Use the 'Business Card' mode of the Manual Probe subsystem.  This is invoked as
      		      G29 P2 B   The mode of G29 P2 allows you to use a bussiness card or recipe card
		      as a shim that the nozzle will pinch as it is lowered.   The idea is that you 
		      can easily feel the nozzle getting to the same height by the amount of resistance
		      the business card exhibits to movement.   You should try to achieve the same amount
		      of resistance on each probed point to facilitate accurate and repeatable measurements.
		      You should be very careful not to drive the nozzle into the bussiness card with a 
		      lot of force as it is very possible to cause damage to your printer if your are
		      careless.  If you use the B option with G29 P2 B you can leave the number parameter off
		      on its first use to enable measurement of the business card thickness.  Subsequent usage
		      of the B parameter can have the number previously measured supplied to the command.
		      Incidently, you are much better off using something like a Spark Gap feeler gauge than
		      something that compresses like a Business Card. 

      C     Continue  Continue, Constant, Current Location.   This is not a primary command.  C is used to 
      		      further refine the behaviour of several other commands.  Issuing a G29 P1 C will 
		      continue the generation of a partially constructed Mesh without invalidating what has 
		      been done.  Issuing a G29 P2 C will tell the Manual Probe subsystem to use the current
		      location in its search for the closest unmeasured Mesh Point.  When used with a G29 Z C 
		      it indicates to use the current location instead of defaulting to the center of the print bed.

      D     Disable   Disable the Unified Bed Leveling system.

      F #   Fade      Fade the amount of Mesh Based Compensation over a specified height.  At the specified height, 
      		      no correction is applied and natural printer kenimatics take over.  If no number is specified 
		      for the command, 10mm is assummed to be reasonable.

      G #   Grid      Perform a Grid Based Leveling of the current Mesh using a grid with n points on
     		      a side.

      H #   Height    Specify the Height to raise the nozzle after each manual probe of the bed.  The
      		      default is 5mm.  

      I #   Invalidate Invalidate specified number of Mesh Points.  The nozzle location is used unless
      		      the X and Y parameter are used.   If no number is specified, only the closest Mesh
		      point to the location is invalidated.  The M parameter is available as well to produce 
		      a map after the operation.  This command is useful to invalidate a portion of the 
		      Mesh so it can be adjusted using other tools in the Unified Bed Leveling System.  When
		      attempting to invalidate an isolated bad point in the mesh, the M option will indicate
		      where the nozzle is positioned in the Mesh with (#).  You can move the nozzle around on
		      the bed and use this feature to select the center of the area (or cell) you want to 
		      invalidate.

      K #   Kompare   Kompare current Mesh with stored Mesh # replacing current Mesh with the result.  This
                      command litterly performs a difference between two Mesh. 

      L     Load      Load Mesh from the previously activated location in the EEPROM.

      L #   Load      Load Mesh from the specified location in the EEPROM.  Set this location as activated
                      for subsequent Load and Store operations.

      M     Map       Display the Mesh Map Topology.  The parameter can be specified alone (ie. G29 M) or
      		      in combination with many of the other commands.  The Mesh Map option works with
		      all of the Phase commands (ie. G29 P4 R 5 X 50 Y100 C -.1 M) 

      N    No Home    G29 normally insists that a G28 has been performed.  You can over rule this with an
      		      N option.  In general, you should not do this.  This can only be done safely with
		      commands that do not move the nozzle.
	
The P or Phase commands are used for the bulk of the work to setup a Mesh.  In general, your Mesh will
start off being initialized with a G29 P0 or a G29 P1.   Further refinement of the Mesh happens with
each additional Phase that processes it.

      P0    Phase 0   Zero Mesh Data and turn off the Mesh Compensation System.  This reverts the
                      Delta Printer to the same state it was in before the Delta Mesh Based Compensation
                      System was turned on.  Setting the entire Mesh to Zero is a special case that allows
                      a subsequent G or T leveling operation for backward compatability.

      P1    Phase 1   Invalidate entire Mesh and continue with automatic generation of the Mesh data using
                      the Z-Probe.   Depending upon the values of DELTA_PROBEABLE_RADIUS and
                      DELTA_PRINTABLE_RADIUS some area of the bed will not have Mesh Data automatically
                      generated.  This will be handled in Phase 2.  If the Phase 1 command is given the 
		      C (Continue) parameter it does not invalidate the Mesh prior to automatically 
		      probing needed locations.  This allows you to invalidate portions of the Mesh but still
		      use the automatic probing capabilities of the Unified Bed Leveling System.  An X and Y
		      parameter can be given to prioritize where the command should be trying to measure points.
		      If the X and Y parameters are not specified the current probe position is used.  Phase 1
		      allows you to specify the M (Map) parameter so you can watch the generation of the Mesh.
		      Phase 1 also watches for the LCD Panel's Encoder Switch being held in a depressed state.
		      It will suspend generation of the Mesh if it sees the user request that.  (This check is
		      only done between probe points.  You will need to press and hold the switch until the
		      Phase 1 command can detect it.)

      P2    Phase 2   Probe areas of the Mesh that can not be automatically handled.  Phase 2 respects an H
      		      parameter to control the height between Mesh points.  The default height for movement
		      between Mesh points is 5mm.  A smaller number can be used to make this part of the 
		      calibration less time consuming.  You will be running the nozzle down until it just barely 
		      touches the glass.  You should have the nozzle clean with no plastic obstructing your view.  
		      Use caution and move slowly.  It is possible to damage your printer if you are careless. 
		      Note that this command will use the configuration #define SIZE_OF_LITTLE_RAISE if the 
		      nozzle is moving a distance of less than BIG_RAISE_NOT_NEEDED. 

		      The H parameter can be set negative if your Mesh dips in a large area.  You can press
		      and hold the LCD Panel's encoder wheel to terminate the current Phase 2 command.  You
		      can then re-issue the G29 P 2 command with an H parameter that is more suitable for the
		      area you are manually probing.   Note that the command tries to start you in a corner
		      of the bed where movement will be predictable.  You can force the location to be used in
		      the distance calculations by using the X and Y parameters.  You may find it is helpful to
		      print out a Mesh Map (G29 M ) to understand where the mesh is invalidated and where
		      the nozzle will need to move in order to complete the command.    The C parameter is 
		      available on the Phase 2 command also and indicates the search for points to measure should
		      be done based on the current location of the nozzle.

		      A B parameter is also available for this command and described up above.  It places the
		      manual probe subsystem into Business Card mode where the thickness of a business care is
		      measured and then used to accurately set the nozzle height in all manual probing for the 
		      duration of the command.  (S for Shim mode would be a better parameter name, but S is needed 
		      for Save or Store of the Mesh to EEPROM)  A Business card can be used, but you will have
		      better results if you use a flexible Shim that does not compress very much.  That makes it 
		      easier for you to get the nozzle to press with similar amounts of force against the shim so you 
		      can get accurate measurements.  As you are starting to touch the nozzle against the shim try
		      to get it to grasp the shim with the same force as when you measured the thickness of the
		      shim at the start of the command.

		      Phase 2 allows the M (Map) parameter to be specified.  This helps the user see the progression
		      of the Mesh being built.

      P3    Phase 3   Fill the unpopulated regions of the Mesh with a fixed value.  The C parameter is used to
      		      specify the Constant value to fill all invalid areas of the Mesh.  If no C parameter is 
		      specified, a value of 0.0 is assumed.  The R parameter can be given to specify the number 
		      of points to set.  If the R parameter is specified the current nozzle position is used to 
		      find the closest points to alter unless the X and Y parameter are used to specify the fill 
		      location.

      P4    Phase 4   Fine tune the Mesh.  The Delta Mesh Compensation System assume the existance of
                      an LCD Panel.  It is possible to fine tune the mesh without the use of an LCD Panel.  
		      (More work and details on doing this later!)
                      The System will search for the closest Mesh Point to the nozzle.  It will move the
                      nozzle to this location.  The user can use the LCD Panel to carefully adjust the nozzle
                      so it is just barely touching the bed.  When the user clicks the control, the System
                      will lock in that height for that point in the Mesh Compensation System.

                      Phase 4 has several additional parameters that the user may find helpful.  Phase 4
                      can be started at a specific location by specifying an X and Y parameter.  Phase 4
                      can be requested to continue the adjustment of Mesh Points by using the R(epeat)
                      parameter.  If the Repetition count is not specified, it is assumed the user wishes
                      to adjust the entire matrix.  The nozzle is moved to the Mesh Point being edited.
		      The command can be terminated early (or after the area of interest has been edited) by
		      pressing and holding the encoder wheel until the system recognizes the exit request.
                      Phase 4's general form is G29 P4 [R # of points] [X position] [Y position]

		      Phase 4 is intended to be used with the G26 Mesh Validation Command.   Using the 
		      information left on the printer's bed from the G26 command it is very straight forward
		      and easy to fine tune the Mesh.   One concept that is important to remember and that 
		      will make using the Phase 4 command easy to use is this:  You are editing the Mesh Points.
		      If you have too little clearance and not much plastic was extruded in an area, you want to
		      LOWER the Mesh Point at the location.  If you did not get good adheasion, you want to
		      RAISE the Mesh Point at that location.


      P5    Phase 5   Find Mean Mesh Height and Standard Deviation.   Typically, it is easier to use and
      		      work with the Mesh if it is Mean Adjusted.  You can specify a C parameter to 
		      Correct the Mesh to a 0.00 Mean Height.   Adding a C parameter will automatically 
		      execute a G29 P6 C <mean height>.   
      
      P6    Phase 6   Shift Mesh height.  The entire Mesh's height is adjusted by the height specified 
      		      with the C parameter.  Being able to adjust the height of a Mesh is useful tool.  It
		      can be used to compensate for poorly calibrated Z-Probes and other errors.  Ideally,
		      you should have the Mesh adjusted for a Mean Height of 0.00 and the Z-Probe measuring
		      0.000 at the Z Home location.

      Q     Test      Load specified Test Pattern to assist in checking correct operation of system.  This 
      		      command is not anticipated to be of much value to the typical user.  It is intended
		      for developers to help them verify correct operation of the Unified Bed Leveling System.

      S     Store     Store the current Mesh in the Activated area of the EEPROM.

      S #   Store     Store the current Mesh at the specified location in EEPROM.  Activate this location
                      for subsequent Load and Store operations.

      T     3-Point   Perform a 3 Point Bed Leveling on the current Mesh

      W     What?     Display valuable data the Unified Bed Leveling System knows.

      X #             Specify X Location for this line of commands

      Y #             Specify Y Location for this line of commands

      Z     Zero      Probes to set the Z Height of the nozzle.  The entire Mesh can be raised or lowered 
                      by just doing a G29 Z

      Z #   Zero      The entire Mesh can be raised or lowered to conform with the specified difference.  
      		      Z_PROBE_OFFSET_FROM_EXTRUDER is added to the calculation.  


     Release Notes:
    			You MUST do a M502 & M500 pair of commands to initialize the storage.  Failure to do this
    			will cause all kinds of problems.  Enabling EEPROM Storage is highly recommended.  With 
			EEPROM Storage of the mesh, you are limited to 3-Point and Grid Leveling.  (G29 P0 T and 
			G29 P0 G respectively.)

    			Z-Probe Sleds are not currently fully supported.  There were too many complications caused
    			by them to support them in the Unified Bed Leveling code.  Support for them will be handled
   			better in the upcoming Z-Probe Object that will happen during the Code Clean Up phase.  (That
   			is what they really are:  A special case of the Z-Probe.)  When a Z-Probe Object appears, it
			should slip in under the Unified Bed Leveling code without major trauma.

			When you do a G28 and then a G29 P1 to automatically build your first mesh, you are going to notice
			the Unified Bed Leveling probes points further and further away from the starting location. (The
			starting location defaults to the center of the bed.)  	The original Grid and Mesh leveling used 
			a Zig Zag pattern. The new pattern is better, especially for people with Delta printers.  This 
			allows you to get the center area of the Mesh populated (and edited) quicker.   This allows you to 
			perform a small print and check out your settings quicker.   You do not need to populate the 
		       	entire mesh to use it.	(You don't want to spend a lot of time generating a mesh only to realize 
			you don't have the resolution or Z_PROBE_OFFSET_FROM_EXTRUDER set correctly.  The Mesh generation
			gathers points closest to where the nozzle is located unless you specify an (X,Y) coordinate pair.

			The Unified Bed Leveling uses a lot of EEPROM storage to hold its data.  And it takes some effort
			to get this Mesh data correct for a user's printer.  We do not want this data destroyed as
			new versions of Marlin add or subtract to the items stored in EEPROM.   So, for the benefit of
			the users, we store the Mesh data at the end of the EEPROM and do not keep it contiguous with the
			other data stored in the EEPROM.  (For sure the developers are going to complain about this, but
			this is going to be helpful to the users!)

			The foundation of this Bed Leveling System is built on Epatel's Mesh Bed Leveling code.  A big 
			'Thanks!' to him and the creators of 3-Point and Grid Based leveling.   Combining thier contributions
			we now have the functionality and features of all three systems combined.
*/

int Unified_Bed_Leveling_EEPROM_start = -1;
int UBL_has_control_of_LCD_Panel = 0;
volatile int G29_encoderDiff = 0;	// This is volatile because it is getting changed at interrupt time.

// We keep the simple parameter flags and values as 'static' because we break out the 
// parameter parsing into a support routine.

static int G29_Verbose_Level = 0, Mesh_Map_Flag = 0, Test_Value = 0;
static int Phase_Value = -1, Repeat_Flag = 0, C_Flag=0, Repetition_Cnt = 1, X_Flag = 0, Y_Flag = 0;
static float X_Pos = 0.0, Y_Pos = 0.0, Height_Value = 5.0, measured_z, card_thickness=0.0;
static float Constant = 0.0;
static int Statistics_Flag = 0, Storage_Slot = 0, Business_Card_Mode = 0, Test_Pattern = 0;

#if ENABLED(ULTRA_LCD)
void lcd_setstatus(const char* message, bool persist);
#endif

void gcode_G29() {
  struct mesh_index_pair location;
  int i, j, k;
  float Z1, Z2, Z3;
 
  G29_Verbose_Level = 0;	// These may change, but let's get some reasonable values into them.
  Repeat_Flag       = 0;
  Repetition_Cnt    = 1;
  C_Flag            = 0;

  if ( Unified_Bed_Leveling_EEPROM_start < 0 ) {
    SERIAL_PROTOCOLLNPGM("?You need to enable your EEPROM and initialize it ");
    SERIAL_PROTOCOLLNPGM("with M502, M500, M501 in that order.\n");
    return;
  }
 
 if ( !code_seen('N') )
 if ( axis_unhomed_error(true, true, true) )	 // Don't allow auto-leveling without homing first
   gcode_G28();

 if ( G29_Parameter_Parsing() )			// if parsing the simple paramters causes a problem,
	 return;				// we abort the command.

  if ( code_seen('I') ) {			// Invalidate Mesh Points     This command is a little bit asymetrical because
	Repetition_Cnt = 1;			// it directly specifies the repetition count and does not use the 'R' parameter.
	if (code_has_value() ) 
		Repetition_Cnt = code_value_int();
	while ( Repetition_Cnt-- ) {
		location = find_closest_mesh_point_of_type( REAL, X_Pos,  Y_Pos, 0, NULL);	// The '0' says we want to use the nozzle's position
		if ( location.x_index < 0 ) {
			SERIAL_PROTOCOLLNPGM("Entire Mesh invalidated.\n");
			break;						// No more invalid Mesh Points to populate
		}
		bed_leveling_mesh.z_values[location.x_index][location.y_index] = NAN;
	}
	SERIAL_PROTOCOLLNPGM("Locations invalidated.\n");
  }


  if ( code_seen('Q') ) {
	if ( code_has_value() )
		Test_Pattern = code_value_int();

	if ( Test_Pattern < 0 || Test_Pattern > 4) {
		SERIAL_PROTOCOLLNPGM("Invalid Test_Pattern value.  (0-4)\n");
		return;
	}
	SERIAL_PROTOCOLLNPGM("Loading Test_Pattern values.\n");
	switch (Test_Pattern) {
	case 0: for(i=0; i<MESH_NUM_X_POINTS; i++ ) {						// Create a bowl shape.  This is
			for(j=0; j<MESH_NUM_Y_POINTS; j++ ) {					// similar to what a user would see with
				Z1 = (((float) MESH_NUM_X_POINTS)/2.0) - i;			// a poorly calibrated Delta.  
				Z2 = (((float) MESH_NUM_Y_POINTS)/2.0) - j;
				bed_leveling_mesh.z_values[i][j] += 2.0 * sqrt( Z1*Z1 + Z2*Z2); 
			}
		}
		break;
	case 1: for(i=0; i<MESH_NUM_X_POINTS; i++ ) {						// Create a diagonal line several Mesh
			bed_leveling_mesh.z_values[i][i] += 10.0 ; 				// cells thick that is raised
			if (i<MESH_NUM_Y_POINTS-1)	
				bed_leveling_mesh.z_values[i][i+1] += 10.0 ; // We want the altered line several mesh points thick
			if (i>0)			
				bed_leveling_mesh.z_values[i][i-1] += 10.0 ; // We want the altered line several mesh points thick
		}
		break;
	case 2: for(i=MESH_NUM_X_POINTS/3.0; i<2*(MESH_NUM_X_POINTS/3.0); i++ ) {		// Create a rectangular raised area in
			for(j=MESH_NUM_Y_POINTS/3.0; j<2*(MESH_NUM_Y_POINTS/3.0); j++ ) {	// the center of the bed
				bed_leveling_mesh.z_values[i][j] += 10.0 ; 
			}
		}
		break;
	case 3:
		break;
	}
  }


  if ( code_seen('P') ) {
    Phase_Value = code_value_int();
    if ( Phase_Value < 0 || Phase_Value > 7) {
      SERIAL_PROTOCOLLNPGM("Invalid Phase value.  (0-4)\n");
      return;
    }
    switch (Phase_Value) {
//
// Zero Mesh Data
//
      case 0:	bed_leveling_mesh.reset();
        	SERIAL_PROTOCOLLNPGM("Mesh zeroed.\n");
        	break;
//
// Invalidate Entire Mesh and Automatically Probe Mesh in areas that can be reached by the probe
//
      case 1:	if ( !code_seen('C') )  {
			bed_leveling_mesh.invalidate();
			SERIAL_PROTOCOLLNPGM("Mesh invalidated.  Probing mesh.\n");
		}
    		if ( G29_Verbose_Level > 1 ) {
			SERIAL_ECHOPAIR("Probing Mesh Points Closest to (",X_Pos);
			SERIAL_ECHOPAIR(",",Y_Pos);
        		SERIAL_PROTOCOLLNPGM(")\n");
		}
		probe_entire_mesh( X_Pos+X_PROBE_OFFSET_FROM_EXTRUDER, Y_Pos+Y_PROBE_OFFSET_FROM_EXTRUDER, code_seen('M') );
        	break;
//
// Manually Probe Mesh in areas that can not be reached by the probe
//
      case 2:	SERIAL_PROTOCOLLNPGM("Manually probing unreachable mesh locations.\n");
        	do_blocking_move_to_z( Z_RAISE_BETWEEN_PROBINGS );
		if ( X_Flag==0 && Y_Flag==0) {			// use a good default location for the path
			X_Pos = X_MIN_POS;
			Y_Pos = Y_MIN_POS;
			if ( X_PROBE_OFFSET_FROM_EXTRUDER > 0 )	// The flipped > and < operators on these two comparisons is
				X_Pos = X_MAX_POS;		// intential.  It should cause the probed points to follow a
			if ( Y_PROBE_OFFSET_FROM_EXTRUDER < 0 )	// nice path on Cartesian printers.  It may make sense to
				Y_Pos = Y_MAX_POS;		// have Delta printers default to the center of the bed.
		}						// For now, until that is decided, it can be forced with the X
								// and Y parameters.
		if ( code_seen('C') ) {
			X_Pos = current_position[X_AXIS];
			Y_Pos = current_position[Y_AXIS];
		}

		Height_Value = Z_RAISE_BETWEEN_PROBINGS;
		if ( code_seen('H') )
		    if ( code_has_value() ) 
  			Height_Value = code_value_float();

		if ( code_seen('B') )  {
			Business_Card_Mode++;
			if (code_has_value() )
				card_thickness = code_value_float();
			else {
				card_thickness = measure_business_card_thickness( Height_Value );
			}
			if ( abs(card_thickness) > 1.5 )  {
				SERIAL_PROTOCOLLNPGM("?Error in Business Card measurment.\n");
				return;
			}
		}
		manually_probe_remaining_mesh( X_Pos, Y_Pos, Height_Value, card_thickness, code_seen('M') );
        	break;
//
// Populate invalid Mesh areas with a constant
//
      case 3:   Height_Value = 0.0;	// Assume 0.0 until proven otherwise
		if (code_seen('C')) {
			Height_Value = Constant;
		}
		if ( !Repeat_Flag ) 			// If no repetition is specified, do the whole Mesh
			Repetition_Cnt = 9999;
		while ( Repetition_Cnt-- ) {
			location = find_closest_mesh_point_of_type( INVALID, X_Pos,  Y_Pos, 0, NULL);	// The '0' says we want to use the nozzle's position
			if ( location.x_index < 0 )
				break;						// No more invalid Mesh Points to populate
			bed_leveling_mesh.z_values[location.x_index][location.y_index] = Height_Value;
		}
		break;
//
// Fine Tune (Or Edit) the Mesh
//
      case 4:  	fine_tune_mesh( X_Pos, Y_Pos, Height_Value, code_seen('M') );
        	break;
      case 5:   Find_Mean_Mesh_Height();
	    	break;
      case 6:   Shift_Mesh_Height( );
        	break;

      case 10: 	UBL_has_control_of_LCD_Panel++;			// Debug code...  Pan no attention to this stuff
     		SERIAL_ECHO_START;
    		SERIAL_ECHO("Checking G29 has control of LCD Panel:\n");
	      	while( !G29_lcd_clicked() ) {
			idle();
	      		delay(250);
	      		SERIAL_PROTOCOL( G29_encoderDiff );
	      		G29_encoderDiff = 0;
      			SERIAL_EOL;
		}
		while ( G29_lcd_clicked() )
			idle();
		UBL_has_control_of_LCD_Panel = 0;;
    		SERIAL_ECHO("G29 giving back control of LCD Panel.\n");
        	break;
    }
  }

  if ( code_seen('T') ) {
     float xxx,yyy,zzz;
	Z1 = probe_pt( UBL_PROBE_PT_1_X, UBL_PROBE_PT_1_Y, false /*Stow Flag*/, G29_Verbose_Level)+Z_PROBE_OFFSET_FROM_EXTRUDER;
	Z2 = probe_pt( UBL_PROBE_PT_2_X, UBL_PROBE_PT_2_Y, false /*Stow Flag*/, G29_Verbose_Level)+Z_PROBE_OFFSET_FROM_EXTRUDER;
	Z3 = probe_pt( UBL_PROBE_PT_3_X, UBL_PROBE_PT_3_Y, true  /*Stow Flag*/, G29_Verbose_Level)+Z_PROBE_OFFSET_FROM_EXTRUDER;

//  We need to adjust Z1, Z2, Z3 by the Mesh Height at these points.  Just because they are non-zero doesn't mean
//  the Mesh is tilted!  (We need to compensate each probe point by what the Mesh says that location's height is)

	Z1 -= bed_leveling_mesh.get_z_correction( UBL_PROBE_PT_1_X, UBL_PROBE_PT_1_Y);	
	Z2 -= bed_leveling_mesh.get_z_correction( UBL_PROBE_PT_2_X, UBL_PROBE_PT_2_Y);	
	Z3 -= bed_leveling_mesh.get_z_correction( UBL_PROBE_PT_3_X, UBL_PROBE_PT_3_Y);

	do_blocking_move_to_xy( (X_MAX_POS-X_MIN_POS)/2.0, (Y_MAX_POS-Y_MIN_POS)/2.0);
	tilt_mesh_based_on_3pts( Z1, Z2, Z3 );
  }

//
// Much of the 'What?' command can be eliminated.  But until we are fully debugged, it is
// good to have the extra information.   Soon... we prune this to just a few items
//
  if ( code_seen('W') ) 
	  G29_What_Command();


//
// When we are fully debugged, the EEPROM dump command will get deleted also.  But
// right now, it is good to have the extra information.   Soon... we prune this.
//
  if ( code_seen('E') ) 
	  G29_EEPROM_Dump();   // EEPROM Dump


//
// Whe we are fully debugged, this may go away.  But there are some valid
// use cases for the users.  So we can wait and see what to do with it.
//

  if ( code_seen('K') ) 			// Kompare Current Mesh Data to Specified Stored Mesh
	G29_Kompare_Current_Mesh_to_Stored_Mesh() ;


//
// Load a Mesh from the EEPROM
//

  if ( code_seen('L') ) {			// Load Current Mesh Data
    Storage_Slot = bed_leveling_mesh.state.EEPROM_storage_slot;
    if ( code_has_value() )
      Storage_Slot = code_value_int();

    k = E2END - sizeof( bed_leveling_mesh.state );
    j = (k - Unified_Bed_Leveling_EEPROM_start) / sizeof( bed_leveling_mesh.z_values );

    if ( Storage_Slot < 0 || Storage_Slot > j || Unified_Bed_Leveling_EEPROM_start <= 0) {
      SERIAL_PROTOCOLLNPGM("?EEPROM storage not available for use.\n");
      return;
    }
    bed_leveling_mesh.load_mesh( Storage_Slot );
    bed_leveling_mesh.state.EEPROM_storage_slot = Storage_Slot;
    bed_leveling_mesh.store_state( );
    SERIAL_PROTOCOLLNPGM("Done.\n");
  }

//
// Store a Mesh in the EEPROM
//

  if ( code_seen('S') ) {			// Store (or Save) Current Mesh Data
    Storage_Slot = bed_leveling_mesh.state.EEPROM_storage_slot;
    if ( code_has_value() )
      Storage_Slot = code_value_int();

    k = E2END - sizeof( bed_leveling_mesh.state );
    j = (k - Unified_Bed_Leveling_EEPROM_start) / sizeof( bed_leveling_mesh.z_values );

    if ( Storage_Slot < 0 || Storage_Slot > j || Unified_Bed_Leveling_EEPROM_start <= 0) {
      SERIAL_PROTOCOLLNPGM("?EEPROM storage not available for use.\n");
      goto LEAVE;
    }
    bed_leveling_mesh.state.EEPROM_storage_slot = Storage_Slot;
    bed_leveling_mesh.store_state( );
    bed_leveling_mesh.store_mesh( Storage_Slot );
    SERIAL_PROTOCOLLNPGM("Done.\n");
  }


  if ( code_seen('M') ) {
    i = 0;
    if (code_has_value())
      i = code_value_int();
    bed_leveling_mesh.display_map(i);
  }

  if ( code_seen('Z') ) {
    if ( code_has_value() )
       bed_leveling_mesh.state.z_offset = code_value_float();		// do the simple case.  Just lock in the specified value
    else {
        save_UBL_active_state_and_disable();
//      measured_z = probe_pt( X_Pos+X_PROBE_OFFSET_FROM_EXTRUDER,  Y_Pos+Y_PROBE_OFFSET_FROM_EXTRUDER, ProbeDeployAndStow, G29_Verbose_Level );

	measured_z = 1.5;
	do_blocking_move_to_z(measured_z);     	// Get close to the bed, but leave some space so we don't damage anything
						// The user is not going to be lockinging in a new Z-Offset very often so
						// it won't be that painful to spin the Encoder Wheel for 1.5mm
	lcd_implementation_clear();
	lcd_z_offset_edit_setup( measured_z );
	do {
		measured_z = lcd_z_offset_edit();
		idle();
		do_blocking_move_to_z(measured_z);     
	} while ( !G29_lcd_clicked() );

	UBL_has_control_of_LCD_Panel = 1;	// There is a race condition for the Encoder Wheel getting clicked.
						// It could get detected in lcd_mesh_edit (actually _lcd_mesh_fine_tune( )
						// or here.  So, until we are done looking for a long Encoder Wheel Press,
						// we need to take control of the panel
	unsigned long cnt;
	cnt = millis();
	lcd_return_to_status();
	while ( G29_lcd_clicked() ) { 		// debounce and watch for abort
		idle();
		if ( millis() - cnt > 1500L ) {
			SERIAL_PROTOCOLLNPGM("\nZ-Offset Adjustment Stopped.");
			do_blocking_move_to_z(Z_RAISE_PROBE_DEPLOY_STOW);     
			lcd_setstatus( "Z-Offset Stopped.   ", true);
  
			while ( G29_lcd_clicked() )  	{
				idle();
			}
			UBL_has_control_of_LCD_Panel = 0; 
			restore_UBL_active_state_and_leave(); 
			goto LEAVE;
		}
	}
	UBL_has_control_of_LCD_Panel = 0; 
	delay(20);	// We don't want any switch noise. 

	bed_leveling_mesh.state.z_offset = measured_z;

	lcd_implementation_clear();
	restore_UBL_active_state_and_leave(); 
    }
  }

LEAVE: 
#if ENABLED(ULTRA_LCD)
	lcd_setstatus( "                         ", true);
	lcd_quick_feedback();
#endif
  	UBL_has_control_of_LCD_Panel = 0;
  	return;
}



void Find_Mean_Mesh_Height()  {
int i, j, n; 
float sum, sum_of_diff_squared, sigma, difference, mean;

	sum = 0.0;
	sum_of_diff_squared = 0.0;
	n = 0;
	for (i = 0; i < MESH_NUM_X_POINTS; i++) {
		for (j = 0;  j < MESH_NUM_Y_POINTS; j++) {
			if ( !isnan( bed_leveling_mesh.z_values[i][j]) ) {
				sum += bed_leveling_mesh.z_values[i][j];
				n++;
			}
		}
	}
    	mean = sum/n ;
//
// Now do the sumation of the squares of difference from mean
//
	for (i = 0; i < MESH_NUM_X_POINTS; i++) {
		for (j = 0;  j < MESH_NUM_Y_POINTS; j++) {
			if ( !isnan( bed_leveling_mesh.z_values[i][j]) ) {
				difference = (bed_leveling_mesh.z_values[i][j] - mean);
				sum_of_diff_squared += difference * difference;
			}
		}
	}
    	SERIAL_ECHOPAIR("# of samples: ", n );
	SERIAL_EOL;
    	SERIAL_ECHO("Mean Mesh Height: ");
    	SERIAL_ECHO_F( mean, 6 );
	SERIAL_EOL;
   
	sigma = sqrt( sum_of_diff_squared / (n + 1) );
    	SERIAL_ECHO("Standard Deviation: ");
    	SERIAL_ECHO_F( sigma, 6 );
	SERIAL_EOL;

	if ( C_Flag) {
		for (i = 0; i < MESH_NUM_X_POINTS; i++) {
			for (j = 0;  j < MESH_NUM_Y_POINTS; j++) {
				if ( !isnan( bed_leveling_mesh.z_values[i][j]) ) {
					bed_leveling_mesh.z_values[i][j] -=  mean + Constant;
				}
			}
		}
	}
}

void Shift_Mesh_Height( )  {
int i, j;
	for (i = 0; i < MESH_NUM_X_POINTS; i++) {
		for (j = 0;  j < MESH_NUM_Y_POINTS; j++) {
			if ( !isnan( bed_leveling_mesh.z_values[i][j]) ) {
				bed_leveling_mesh.z_values[i][j] += Constant;
			}
		}
	}
}


// probe_entire_mesh( X_Pos, Y_Pos )  probes all invalidated locations of the mesh that can be reached
// by the probe.  It attempts to fill in locations closest to the nozzle's start location first.

void probe_entire_mesh( float X_Pos, float Y_Pos, bool do_mesh_map )  {
struct mesh_index_pair location;
float xProbe, yProbe, measured_z;

    UBL_has_control_of_LCD_Panel++;
    save_UBL_active_state_and_disable();	 // we don't do bed level correction because we want the raw data when we probe
    DEPLOY_PROBE();

    do {
	if ( G29_lcd_clicked() ) {
    		SERIAL_PROTOCOLLNPGM("\nMesh only partially populated.");
		lcd_quick_feedback();
		while ( G29_lcd_clicked() )
		       idle();	
    		UBL_has_control_of_LCD_Panel = 0;
		STOW_PROBE();
		restore_UBL_active_state_and_leave();
		return;
	}
	location = find_closest_mesh_point_of_type( INVALID, X_Pos,  Y_Pos, 1, NULL);  // the '1' says we want the location to be relative to the probe
    	if (location.x_index>=0 && location.y_index>=0 ) {
		xProbe = bed_leveling_mesh.map_x_index_to_bed_location(location.x_index); 
		yProbe = bed_leveling_mesh.map_y_index_to_bed_location(location.y_index);
		if ( xProbe<MIN_PROBE_X || xProbe>MAX_PROBE_X || yProbe<MIN_PROBE_Y || yProbe>MAX_PROBE_Y)  {
    			SERIAL_PROTOCOLLNPGM("?Error: Attempt to probe off the bed.");
    			UBL_has_control_of_LCD_Panel = 0;
			goto LEAVE;
		}
		measured_z = probe_pt(xProbe, yProbe, ProbeStay, G29_Verbose_Level);
		bed_leveling_mesh.z_values[location.x_index][location.y_index] = measured_z + Z_PROBE_OFFSET_FROM_EXTRUDER;
	}

	if ( do_mesh_map )
    		bed_leveling_mesh.display_map(1);

    } while (location.x_index>=0 && location.y_index>=0 );

LEAVE:
    STOW_PROBE();
    restore_UBL_active_state_and_leave();
    do_blocking_move_to_xy(X_Pos, Y_Pos);
}

 

struct vector tilt_mesh_based_on_3pts(float pt1, float pt2, float pt3) {
	struct vector v1, v2, normal;
	float c, d;
	int i, j;

	v1.dx = (UBL_PROBE_PT_1_X - UBL_PROBE_PT_2_X);
	v1.dy = (UBL_PROBE_PT_1_Y - UBL_PROBE_PT_2_Y);
	v1.dz = (pt1 - pt2);

	v2.dx = (UBL_PROBE_PT_3_X - UBL_PROBE_PT_2_X);
	v2.dy = (UBL_PROBE_PT_3_Y - UBL_PROBE_PT_2_Y);
	v2.dz = (pt3 - pt2);

	// do cross product

	normal.dx = v1.dy * v2.dz - v1.dz * v2.dy;
	normal.dy = v1.dz * v2.dx - v1.dx * v2.dz;
	normal.dz = v1.dx * v2.dy - v1.dy * v2.dx;

	// printf("[%f,%f,%f]    ", normal.dx, normal.dy, normal.dz);

	normal.dx /= normal.dz;	// This code does two things.  This vector is normal to the tilted plane.
	normal.dy /= normal.dz;	// However, we don't know its direction.  We need it to point up.  So if	
	normal.dz /= normal.dz;	// Z is negative, we need to invert the sign of all components of the vector
				// We also need Z to be unity because we are going to be treating this triangle
				// as the sin() and cos() of the bed's tilt
	
	//
	// All of 3 of these points should give us the same d constant
	//

	d = normal.dx*UBL_PROBE_PT_1_X + normal.dy*UBL_PROBE_PT_1_Y + normal.dz*pt1;	
	c = -((normal.dx*UBL_PROBE_PT_1_X + normal.dy*UBL_PROBE_PT_1_Y) - d);
SERIAL_ECHO("d from first  point: ");
SERIAL_ECHO_F(d,6);
SERIAL_ECHO("  c: ");
SERIAL_ECHO_F(c,6);
SERIAL_ECHO("\n");
	d = normal.dx*UBL_PROBE_PT_2_X + normal.dy*UBL_PROBE_PT_2_Y + normal.dz*pt2;	
	c = -((normal.dx*UBL_PROBE_PT_2_X + normal.dy*UBL_PROBE_PT_2_Y) - d);
SERIAL_ECHO("d from second point: ");
SERIAL_ECHO_F(d,6);
SERIAL_ECHO("  c: ");
SERIAL_ECHO_F(c,6);
SERIAL_ECHO("\n");
	d = normal.dx*UBL_PROBE_PT_3_X + normal.dy*UBL_PROBE_PT_3_Y + normal.dz*pt3;	
	c = -((normal.dx*UBL_PROBE_PT_3_X + normal.dy*UBL_PROBE_PT_3_Y) - d);
SERIAL_ECHO("d from third  point: ");
SERIAL_ECHO_F(d,6);
SERIAL_ECHO("  c: ");
SERIAL_ECHO_F(c,6);
SERIAL_ECHO("\n");


	for (i = 0; i < MESH_NUM_X_POINTS; i++) {
		for (j = 0;  j < MESH_NUM_Y_POINTS; j++) {
			c = -((normal.dx*(MESH_MIN_X+i*MESH_X_DIST) + normal.dy*(MESH_MIN_Y+j*MESH_Y_DIST)) - d);
			bed_leveling_mesh.z_values[i][j] += c;
		}
	}

	return normal;
}

float use_encoder_wheel_to_measure_point() {
	UBL_has_control_of_LCD_Panel++;
	while (!G29_lcd_clicked() ) { 		// we need the loop to move the nozzle based on the encoder wheel here!
		idle();
		if ( G29_encoderDiff != 0) {
			float new_z;					
			// We define a new variable so we can know ahead of time where we are trying to go.
			// The reason is we want G29_encoderDiff cleared so an interrupt can update it even before the move 
			// is complete.  (So the dial feels responsive to user)
			new_z = current_position[Z_AXIS]+((float) G29_encoderDiff)/100.0;
			G29_encoderDiff = 0;			
			do_blocking_move_to_z( new_z );
		}	
	}
	while ( G29_lcd_clicked() ) { 		// debounce and wait
		idle();
	}
	UBL_has_control_of_LCD_Panel--;
	return current_position[Z_AXIS];
}

float measure_business_card_thickness(float Height_Value ) {
float Z1, Z2;

	UBL_has_control_of_LCD_Panel++;
        save_UBL_active_state_and_disable();	 // we don't do bed level correction because we want the raw data when we probe

	SERIAL_PROTOCOLLNPGM("Place Shim Under Nozzle and Perform Measurement.");
	do_blocking_move_to_z( Height_Value );
	do_blocking_move_to_xy( (float) ((X_MAX_POS-X_MIN_POS)/2.0), (float) ((Y_MAX_POS-Y_MIN_POS)/2.0) );
				//	, min( planner.max_feedrate[X_AXIS], planner.max_feedrate[Y_AXIS])/2.0 ); 

	Z1 = use_encoder_wheel_to_measure_point();
	do_blocking_move_to_z( current_position[Z_AXIS] + SIZE_OF_LITTLE_RAISE );
	UBL_has_control_of_LCD_Panel = 0;

	SERIAL_PROTOCOLLNPGM("Remove Shim and Measure Bed Height.");
	Z2 = use_encoder_wheel_to_measure_point();
	do_blocking_move_to_z( current_position[Z_AXIS] + SIZE_OF_LITTLE_RAISE );

	if ( G29_Verbose_Level > 1) {
		SERIAL_ECHO("Business Card is: ");
		SERIAL_ECHO_F( abs(Z1-Z2), 6 );
    		SERIAL_PROTOCOLLNPGM("mm thick.");
	}
   	restore_UBL_active_state_and_leave();
	return abs(Z1-Z2);
}


void manually_probe_remaining_mesh( float X_Pos, float Y_Pos, float z_clearance, float card_thickness, bool do_mesh_map ) {
struct mesh_index_pair location;
float last_x, last_y, dx, dy;
float xProbe, yProbe, measured_z;
unsigned long cnt;

    UBL_has_control_of_LCD_Panel++;
    last_x = -9999.99;
    last_y = -9999.99;
    save_UBL_active_state_and_disable();	 // we don't do bed level correction because we want the raw data when we probe
    do_blocking_move_to_z( z_clearance );     
    do_blocking_move_to_xy( X_Pos, Y_Pos );

    do {
	if ( do_mesh_map )
    		bed_leveling_mesh.display_map(1);

	location = find_closest_mesh_point_of_type( INVALID, X_Pos,  Y_Pos, 0, NULL);	// The '0' says we want to use the nozzle's position
											// It doesn't matter if the probe can not reach the 
											// NAN location.  This is a manual probe.
    	if (location.x_index<0 && location.y_index<0 ) 
		continue;

	xProbe = bed_leveling_mesh.map_x_index_to_bed_location(location.x_index); 
	yProbe = bed_leveling_mesh.map_y_index_to_bed_location(location.y_index);
	if ( xProbe<X_MIN_POS || xProbe>X_MAX_POS || yProbe<Y_MIN_POS || yProbe>Y_MAX_POS)  {
		SERIAL_PROTOCOLLNPGM("?Error: Attempt to probe off the bed.");
		UBL_has_control_of_LCD_Panel = 0;
		goto LEAVE;
	}

	dx = xProbe - last_x;
	dy = yProbe - last_y;

        if ( sqrt(dx*dx+dy*dy) < BIG_RAISE_NOT_NEEDED ) 
		do_blocking_move_to_z( current_position[Z_AXIS] + SIZE_OF_LITTLE_RAISE );
	else 
		do_blocking_move_to_z(z_clearance);

	last_x = xProbe;
	last_y = yProbe;
	do_blocking_move_to_xy( xProbe, yProbe );

	while (!G29_lcd_clicked() ) { 		// we need the loop to move the nozzle based on the encoder wheel here!
		idle();
		if ( G29_encoderDiff != 0) {
			float new_z;					
			// We define a new variable so we can know ahead of time where we are trying to go.
			// The reason is we want G29_encoderDiff cleared so an interrupt can update it even before the move 
			// is complete.  (So the dial feels responsive to user)
			new_z = current_position[Z_AXIS]+((float) G29_encoderDiff)/100.0;
			G29_encoderDiff = 0;			
			do_blocking_move_to_z( new_z );
		}	
	}

	cnt = millis();
	while ( G29_lcd_clicked() ) { 		// debounce and watch for abort
		idle();
		if (millis()-cnt > 1500L ) {
			SERIAL_PROTOCOLLNPGM("\nMesh only partially populated.");
			do_blocking_move_to_z(Z_RAISE_PROBE_DEPLOY_STOW);     
			lcd_quick_feedback();
			while ( G29_lcd_clicked() )  	
				idle();
			UBL_has_control_of_LCD_Panel = 0;
   			restore_UBL_active_state_and_leave();
			return;
		}
	}

	bed_leveling_mesh.z_values[location.x_index][location.y_index] = current_position[Z_AXIS] - card_thickness;
	if (G29_Verbose_Level > 2) {
		SERIAL_PROTOCOL("Mesh Point Measured at: ");
		SERIAL_PROTOCOL_F( bed_leveling_mesh.z_values[location.x_index][location.y_index], 6 );
		SERIAL_PROTOCOL("\n");
	}
    } while (location.x_index>=0 && location.y_index>=0 );

    if ( do_mesh_map )
   	bed_leveling_mesh.display_map(1);
LEAVE:
    restore_UBL_active_state_and_leave();
    do_blocking_move_to_z(Z_RAISE_PROBE_DEPLOY_STOW);     
    do_blocking_move_to_xy(X_Pos, Y_Pos);     
}


bool G29_Parameter_Parsing() {

#if ENABLED(ULTRA_LCD)
lcd_setstatus( "Doing G29 UBL !", true);
lcd_quick_feedback();
#endif

  X_Pos = current_position[X_AXIS];
  Y_Pos = current_position[Y_AXIS];
  X_Flag = 0;
  Y_Flag = 0;
  Repeat_Flag = 0;
  Constant = 0.0;
  Repetition_Cnt = 1;

  if ( code_seen('X') ) {
    X_Flag++;
    X_Pos = code_value_float();
    if ( X_Pos < X_MIN_POS || X_Pos > X_MAX_POS) {
      SERIAL_PROTOCOLLNPGM("Invalid X location specified.\n");
      return true;
    }
  }

  if ( code_seen('Y') ) {
    Y_Flag++;
    Y_Pos = code_value_float();
    if ( Y_Pos < Y_MIN_POS || Y_Pos > Y_MAX_POS) {
      SERIAL_PROTOCOLLNPGM("Invalid Y location specified.\n");
      return true;
    }
  }

  if ( (Y_Flag && X_Flag == 0) || (X_Flag && Y_Flag == 0) ) {
    SERIAL_PROTOCOLLNPGM("Both X & Y locations must be specified.\n");
    return true;
  }

  G29_Verbose_Level = 0;
  if ( code_seen('V') ) {
    G29_Verbose_Level = code_value_int();
    if ( G29_Verbose_Level < 0 || G29_Verbose_Level > 4) {
      SERIAL_PROTOCOLLNPGM("Invalid Verbose Level specified.  (0-4)\n");
      return true;
    }
  }

  if ( code_seen('A') ) {			// Activate the Unified Bed Leveling System
    bed_leveling_mesh.state.active = 1;
    SERIAL_PROTOCOLLNPGM("Unified Bed Leveling System activated.\n");
  }

  if ( code_seen('C') ) {	
	  C_Flag++;
	  if ( code_has_value() ) {
		  Constant = code_value_float();
	  }
  }

  if ( code_seen('D') ) {			// Disable the Unified Bed Leveling System
    bed_leveling_mesh.state.active = 0;
    SERIAL_PROTOCOLLNPGM("Unified Bed Leveling System de-activated.\n");
  }

  if ( code_seen('F') ) {
    bed_leveling_mesh.state.G29_Correction_Fade_Height = 10.00;
    if (code_has_value() ) {
      bed_leveling_mesh.state.G29_Correction_Fade_Height = code_value_float();
    }
    if (bed_leveling_mesh.state.G29_Correction_Fade_Height<0.0 || bed_leveling_mesh.state.G29_Correction_Fade_Height>100.0) {
    	SERIAL_PROTOCOLLNPGM("?Bed Level Correction Fade Height Not Plausable.\n");
    	bed_leveling_mesh.state.G29_Correction_Fade_Height = 10.00;
	return true;
    }
  }

  if ( code_seen('R') ) {
    Repeat_Flag++;
    if (code_has_value() ) {
      Repetition_Cnt = code_value_int();
    } else
      Repetition_Cnt = 9999;
    if ( Repetition_Cnt < 1) {
      SERIAL_PROTOCOLLNPGM("Invalid Repetition count.\n");
      return true;
    }
  }

  return false;
}


//
// This function goes away after G29 debug is complete.   But for right now, it is a handy
// routine to dump binary data structures.
//
void dump( char *str, float f )
{
  int i;
  float g;
  char *ptr;

  g = INFINITY;
  g = -g;
  SERIAL_PROTOCOL( str );
  SERIAL_PROTOCOL_F( f, 8 );
  SERIAL_PROTOCOL( "  " );
  ptr = (char *) &f;
  for (i = 0; i < 4; i++) {
    SERIAL_PROTOCOL( "  " );
    prt_hex_byte( *ptr++);
  }
  SERIAL_PROTOCOL( "  isnan()=" );
  SERIAL_PROTOCOL( isnan(f) );
  SERIAL_PROTOCOL( "  isinf()=" );
  SERIAL_PROTOCOL( isinf(f) );
  if ( f == g)
    SERIAL_PROTOCOL( "  Minus Infinity detected." );

  SERIAL_PROTOCOL( "\n" );
}

// These are some primative debug routines that will get stripped out later when the code is solid
// I have some LED's wired up to my RAMPS v1.4 board that let me turn them on and off.  I use them
// for different things as I debugged the code.   I would turn them on at certain points in the code
// or I would light up more or less of them depending upon the value of a variable.   I some times
// used the white one to indicate a + or - value of the Mesh Point being used in the calculations.

// Pin 44  White LED  Top
// Pin 63  Red   LED  Right
// Pin 64  Red   LED  Left
//
 
void status_LED( int pin, int action)
{
static int pin_63 = -1, pin_65 = -1, pin_66 = -1;

	switch (pin) {
	case 63:	if (pin_63== -1){		// if so, we need to initialize the mode to output
				pin_63 = 0x00;		// toggle the pin's status to simpile On/Off mode
    				pinMode(63, OUTPUT);
			}
			if (action==0) {
       				digitalWrite(63, 0);
				pin_63 = 0;	// remember the pin is turned off
				return;
			}
			if (action==1) {
       				digitalWrite(63, 255);
				pin_63 = 1;	// remember the pin is turned on
				return;
			}
			if (pin_63) { // toggle the pin's status
				pin_63 = 0;
				digitalWrite(63, 0);
			} else {
				pin_63 = 1;
				digitalWrite(63, 255);
			}
			break;

	case 65:	if (pin_65== -1){		// if so, we need to initialize the mode to output
				pin_65 = 0x00;		// toggle the pin's status to simpile On/Off mode
    				pinMode(65, OUTPUT);
			}
			if (action==0) {
       				digitalWrite(65, 0);
				pin_65 = 0;	// remember the pin is turned off
				return;
			}
			if (action==1) {
       				digitalWrite(65, 255);
				pin_65 = 1;	// remember the pin is turned on
				return;
			}
			if (pin_65) { // toggle the pin's status
				pin_65 = 0;
				digitalWrite(65, 0);
			} else {
				pin_65 = 1;
				digitalWrite(65, 255);
			}
			break;
	case 66:	if (pin_66== -1){		// if so, we need to initialize the mode to output
				pin_66 = 0x00;		// toggle the pin's status to simpile On/Off mode
    				pinMode(66, OUTPUT);
			}
			if (action==0) {
       				digitalWrite(66, 0);
				pin_66 = 0;	// remember the pin is turned off
				return;
			}
			if (action==1) {
       				digitalWrite(66, 255);
				pin_66 = 1;	// remember the pin is turned on
				return;
			}
			if (pin_66) { // toggle the pin's status
				pin_66 = 0;
				digitalWrite(66, 0);
			} else {
				pin_66 = 1;
				digitalWrite(66, 255);
			}
			break;

	default:	SERIAL_ECHOPAIR("?Unable to take action on LED pin: ", pin);
			SERIAL_ECHO(" \n");
			break;
	}
	return;
}

static int UBL_state_at_invokation = 0;
static int UBL_state_recursion_chk = 0;

void save_UBL_active_state_and_disable()  
{
   UBL_state_recursion_chk++;
   if ( UBL_state_recursion_chk != 1) {
      	SERIAL_ECHO("save_UBL_active_state_and_disabled() called multiple times in a row. \n");
	lcd_setstatus( "save_UBL_active() error", true);
	lcd_quick_feedback();	
	return;
   }
   UBL_state_at_invokation = bed_leveling_mesh.state.active;
   bed_leveling_mesh.state.active = 0;
   return;
}

void restore_UBL_active_state_and_leave()  
{
   UBL_state_recursion_chk--;
   if ( UBL_state_recursion_chk != 0) {
      	SERIAL_ECHO("restore_UBL_active_state_and_leave() called too many times. \n");
	lcd_setstatus( "restore_UBL_active() error", true);
	lcd_quick_feedback();
	return;
   }
   bed_leveling_mesh.state.active = UBL_state_at_invokation;
   return;
}


//
// Much of the 'What?' command can be eliminated.  But until we are fully debugged, it is
// good to have the extra information.   Soon... we prune this to just a few items
//
void G29_What_Command() {
    int k;
    k = E2END - Unified_Bed_Leveling_EEPROM_start;
    Statistics_Flag++;
    SERIAL_PROTOCOLPGM("Unified Bed Leveling System ");
    if ( bed_leveling_mesh.state.active )
    	SERIAL_PROTOCOLLNPGM("Active.");
    else
    	SERIAL_PROTOCOLLNPGM("Inactive.");
    SERIAL_PROTOCOLPGM("\n");

    if ( bed_leveling_mesh.state.EEPROM_storage_slot == 0xffff )
    	SERIAL_PROTOCOLPGM("No Mesh Loaded.\n");
    else {
        SERIAL_PROTOCOLPGM("Mesh: ");
        prt_hex_word( bed_leveling_mesh.state.EEPROM_storage_slot );
        SERIAL_PROTOCOLPGM(" Loaded.  \n");
    }

    SERIAL_ECHOPAIR("\nG29_Correction_Fade_Height : ", bed_leveling_mesh.state.G29_Correction_Fade_Height );
    SERIAL_PROTOCOLPGM("\n");
    idle();


    SERIAL_ECHO("z_offset: ");
    SERIAL_ECHO_F( bed_leveling_mesh.state.z_offset, 6 );
    SERIAL_PROTOCOLPGM("\n");

    SERIAL_ECHOPAIR("UBL_state_at_invokation :", UBL_state_at_invokation);
    SERIAL_PROTOCOLPGM("\n");
    SERIAL_ECHOPAIR("UBL_state_recursion_chk :", UBL_state_recursion_chk);
    SERIAL_PROTOCOLPGM("\n");

    SERIAL_PROTOCOLPGM("\n");
    SERIAL_PROTOCOLPGM("Free EEPROM space starts at: 0x");
    prt_hex_word( Unified_Bed_Leveling_EEPROM_start );
    SERIAL_PROTOCOLPGM("\n");
    idle();

    SERIAL_PROTOCOLPGM("end of EEPROM              : ");
    prt_hex_word( E2END );
    SERIAL_PROTOCOLPGM("\n");
    idle();

    SERIAL_PROTOCOLPGM("z_value[][] size: ");
    SERIAL_PROTOCOL( sizeof(bed_leveling_mesh ) );
    SERIAL_PROTOCOLPGM("  ");
    SERIAL_PROTOCOL( sizeof(bed_leveling_mesh.z_values ) );
    SERIAL_PROTOCOLLNPGM("\n");

    SERIAL_PROTOCOLPGM("EEPROM free for UBL: 0x");
    prt_hex_word( k );
    SERIAL_PROTOCOLPGM("\n");
    idle();

    SERIAL_PROTOCOLPGM("EEPROM can hold 0x");
    prt_hex_word( k / sizeof(bed_leveling_mesh.z_values) );
    SERIAL_PROTOCOLPGM(" meshes. \n");

    SERIAL_PROTOCOLPGM("sizeof(stat)     :");
    prt_hex_word( sizeof(bed_leveling_mesh.state) );
    SERIAL_PROTOCOLPGM("\n");
    idle();

    SERIAL_ECHOPAIR("\nMESH_NUM_X_POINTS  ", MESH_NUM_X_POINTS );
    SERIAL_ECHOPAIR("\nMESH_NUM_Y_POINTS  ", MESH_NUM_Y_POINTS );
    SERIAL_ECHOPAIR("\nMESH_MIN_X         ", MESH_MIN_X );
    SERIAL_ECHOPAIR("\nMESH_MIN_Y         ", MESH_MIN_Y );
    SERIAL_ECHOPAIR("\nMESH_MAX_X         ", MESH_MAX_X );
    SERIAL_ECHOPAIR("\nMESH_MAX_Y         ", MESH_MAX_Y );
    SERIAL_ECHO("\nMESH_X_DIST        ");
    SERIAL_ECHO_F( MESH_X_DIST, 6 );
    SERIAL_ECHO("\nMESH_Y_DIST        ");
    SERIAL_ECHO_F( MESH_Y_DIST, 6 );
    SERIAL_PROTOCOLPGM("\n");
    idle();

    if ( bed_leveling_mesh.sanity_check() == 0)
      SERIAL_PROTOCOLPGM("Unified Bed Leveling sanity checks passed.\n");
    return;
}
//
// When we are fully debugged, the EEPROM dump command will get deleted also.  But
// right now, it is good to have the extra information.   Soon... we prune this.
//

void G29_EEPROM_Dump() {
unsigned char cccc;
int i, j, kkkk;

    SERIAL_ECHO_START;
    SERIAL_ECHO("EEPROM Dump:\n");
    for (i = 0; i < E2END + 1; i += 16) {
      if ( i%4 == 0 )
	idle();
      prt_hex_word( i );
      SERIAL_ECHO(": ");
      for (j = 0; j < 16; j++) {
        kkkk = i + j;
        eeprom_read_block( &cccc, (void *) kkkk, 1 );
        prt_hex_byte( cccc );
        SERIAL_ECHO(' ');
      }
      SERIAL_EOL;
    }
    SERIAL_EOL;
    return;
}


//
// Whe we are fully debugged, this may go away.  But there are some valid
// use cases for the users.  So we can wait and see what to do with it.
//
void G29_Kompare_Current_Mesh_to_Stored_Mesh()  {
    float tmp_z_values[MESH_NUM_X_POINTS][MESH_NUM_Y_POINTS];
    int i, j, k;

    if ( !code_has_value() )  {
      SERIAL_PROTOCOLLNPGM("?Mesh # required.\n");
      return;
    }
    Storage_Slot = code_value_int();

    k = E2END - sizeof( bed_leveling_mesh.state );
    j = (k - Unified_Bed_Leveling_EEPROM_start) / sizeof( tmp_z_values );

    if ( Storage_Slot < 0 || Storage_Slot > j || Unified_Bed_Leveling_EEPROM_start <= 0) {
        SERIAL_PROTOCOLLNPGM("?EEPROM storage not available for use.\n");
	return;
    }

    j = k-(Storage_Slot+1)*sizeof(tmp_z_values);	
    eeprom_read_block( (void *) &tmp_z_values , (void *) j, sizeof(tmp_z_values) );

    SERIAL_ECHOPAIR("Subtracting Mesh ", Storage_Slot);
    SERIAL_PROTOCOLPGM(" loaded from EEPROM address ");		// Soon, we can remove the extra clutter of printing
    prt_hex_word(j);						// the address in the EEPROM where the Mesh is stored.
    SERIAL_PROTOCOLPGM("\n");
    for(i=0; i<MESH_NUM_X_POINTS; i++) 
    	for(j=0; j<MESH_NUM_Y_POINTS; j++) 
    		bed_leveling_mesh.z_values[i][j] = bed_leveling_mesh.z_values[i][j] - tmp_z_values[i][j];
  }



struct mesh_index_pair find_closest_mesh_point_of_type(Mesh_Point_Type type, float X, float Y, bool probe_as_reference, unsigned int bits[16]) {
  int i, j;
  float f, px, py, mx, my, dx, dy, closest=99999.99;
  float current_x, current_y, distance;
  struct mesh_index_pair return_val;

	return_val.x_index = -1;
	return_val.y_index = -1;

	current_x = current_position[X_AXIS];
	current_y = current_position[Y_AXIS];

	px = X;				// Get our reference position.  Either the nozzle or
	py = Y;				// the probe location.
	if ( probe_as_reference) {
		px = X-X_PROBE_OFFSET_FROM_EXTRUDER;
		py = Y-Y_PROBE_OFFSET_FROM_EXTRUDER;
	}

	for(i=0; i<MESH_NUM_X_POINTS; i++) {
		for(j=0; j<MESH_NUM_Y_POINTS; j++) {

			if ( (type==INVALID && isnan(bed_leveling_mesh.z_values[i][j])) || 	// Check to see if this location holds the right thing
				(type==REAL && !isnan(bed_leveling_mesh.z_values[i][j])) ||
				(type==SET_IN_BITMAP && is_bit_set(bits, i, j ))  ) {

				// We only get here if we found a Mesh Point of the specified type

				mx = bed_leveling_mesh.map_x_index_to_bed_location(i);// Check if we can probe this mesh location
				my = bed_leveling_mesh.map_y_index_to_bed_location(j);

				if ( probe_as_reference) {			// If we are using the probe as the reference
					if ( mx<MIN_PROBE_X || mx>MAX_PROBE_X)	// there are some locations we can't get to.
						continue;			// We prune these out of the list and ignore
					if ( my<MIN_PROBE_Y || my>MAX_PROBE_Y)	// them until the next Phase where we do the 
						continue;			// manual nozzle probing.
				}
				dx = px-mx;					// We can get to it.  Let's see if it is the
				dy = py-my;					// closest location to the nozzle.
				distance = sqrt(dx*dx+dy*dy);		

				dx = current_x - mx;				// We are going to add in a weighting factor that considers
				dy = current_y - my;				// the current location of the nozzle.  If two locations are equal
				distance += sqrt(dx*dx+dy*dy)/100.0;		// distance from the measurement location, we are going to give

				if ( distance < closest ) {	
					closest = distance;		// We found a closer location with 
					return_val.x_index = i;		// the specified type of mesh value.
					return_val.y_index = j;	
					return_val.distance= closest;	
				}
			}
		}
	}
	return return_val;
}


void fine_tune_mesh( float X_Pos, float Y_Pos, float Height_Value, bool do_mesh_map ) {
struct mesh_index_pair location;
float xProbe, yProbe, measured_z, new_z;					
unsigned int i, not_done[16];
long round_off;
unsigned long cnt;

    save_UBL_active_state_and_disable();
    for(i=0; i<16; i++) not_done[i]=0xffff;
#if ENABLED(ULTRA_LCD)
    lcd_setstatus( "Fine Tuning Mesh.", true);
#endif

    do_blocking_move_to_z( Z_RAISE_PROBE_DEPLOY_STOW );     
    do_blocking_move_to_xy( X_Pos, Y_Pos );
    do {
	if ( do_mesh_map )
    		bed_leveling_mesh.display_map(1);

	location = find_closest_mesh_point_of_type( SET_IN_BITMAP, X_Pos,  Y_Pos, 0, not_done);	// The '0' says we want to use the nozzle's position
												// It doesn't matter if the probe can not reach this 
												// location.  This is a manual edit of the Mesh Point.
    	if (location.x_index<0 && location.y_index<0 ) 
		continue;						// abort if we can't find any more points.

	bit_clear( not_done, location.x_index, location.y_index );	// Mark this location as 'adjusted' so we will find a
									// different location the next time through the loop

	xProbe = bed_leveling_mesh.map_x_index_to_bed_location(location.x_index); 
	yProbe = bed_leveling_mesh.map_y_index_to_bed_location(location.y_index);
	if ( xProbe<X_MIN_POS || xProbe>X_MAX_POS || yProbe<Y_MIN_POS || yProbe>Y_MAX_POS)  {	// In theory, we don't need this check.
		SERIAL_PROTOCOLLNPGM("?Error: Attempt to edit off the bed.");			// This really can't happen, but for now,
		UBL_has_control_of_LCD_Panel = 0;						// Let's do the check.
		goto FINE_TUNE_EXIT;
	}

	do_blocking_move_to_z( Z_RAISE_PROBE_DEPLOY_STOW );	// Move the nozzle to where we are going to edit
	do_blocking_move_to_xy( xProbe, yProbe );
	new_z = bed_leveling_mesh.z_values[location.x_index][location.y_index] + .001 ;

	round_off = (long int) ((new_z+.0025)*1000.0);		// we chop off the last digits just to be clean.  We are rounding to the
	round_off = round_off - (round_off % 5l);		// closest 0 or 5 at the 3rd decimal place.
	new_z = ((float) (round_off))/1000.0;			

SERIAL_ECHO("Mesh Point Currently At:  ");
SERIAL_ECHO_F( new_z, 6 );
SERIAL_ECHO("\n");

	lcd_implementation_clear();
	lcd_mesh_edit_setup( new_z );
	do {
		new_z = lcd_mesh_edit();
		idle();
	} while ( !G29_lcd_clicked() );

	UBL_has_control_of_LCD_Panel = 1;	// There is a race condition for the Encoder Wheel getting clicked.
						// It could get detected in lcd_mesh_edit (actually _lcd_mesh_fine_tune( )
						// or here.  So, until we are done looking for a long Encoder Wheel Press,
						// we need to take control of the panel
	cnt = millis();
	lcd_return_to_status();
	while ( G29_lcd_clicked() ) { 		// debounce and watch for abort
		idle();
		if ( millis() - cnt > 1500L ) {
			SERIAL_PROTOCOLLNPGM("\nFine Tuning of Mesh Stopped.");
			do_blocking_move_to_z(Z_RAISE_PROBE_DEPLOY_STOW);     
			lcd_setstatus( "Mesh Editing Stopped.   ", true);
  
			while ( G29_lcd_clicked() )  	{
				idle();
			}
			UBL_has_control_of_LCD_Panel = 0; 
			goto FINE_TUNE_EXIT;
		}
	}
	UBL_has_control_of_LCD_Panel = 0; 
	delay(20);	// We don't want any switch noise. 

	bed_leveling_mesh.z_values[location.x_index][location.y_index] = new_z;

	lcd_implementation_clear();

    } while (location.x_index>=0 && location.y_index>=0 && --Repetition_Cnt );

FINE_TUNE_EXIT:
    if ( do_mesh_map )
   	bed_leveling_mesh.display_map(1);
    restore_UBL_active_state_and_leave();
    do_blocking_move_to_z(Z_RAISE_PROBE_DEPLOY_STOW);     
    do_blocking_move_to_xy(X_Pos, Y_Pos);     

    UBL_has_control_of_LCD_Panel = 0;

    #if ENABLED(ULTRA_LCD)
       lcd_setstatus( "Done Editing Mesh.   ", true);
    #endif
    SERIAL_ECHO("Done Editing Mesh. \n");
    return;
}



#endif


