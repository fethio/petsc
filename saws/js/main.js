//Make an array. Each array element (matInfo[0]. etc) will have all of the information of the questions.
var matInfo = [];
var matInfoWriteCounter = 0;//next available space to write to.
var currentAsk = "0";//start at id=0. then 00 01, then 000 001 010 011 etc if splitting two every time.
var askedA0 = false;//a one-way flag to record if A0 was asked
var finishedAsking = false;//whether input form has finished (when finished, stop pulling default options from sawsInfo?)

//variables used to collect saws information
var sawsInfo = [];
var sawsInfoWriteCounter = 0;//next available space to write to for fieldsplits (new A divs)
var sawsDataWriteCounter = 0;//next available space to write to for ksp/pc options
var currentFieldsplitWord = "";//temperature, omega, etc

//Use for pcmg
var mgLevelLocation = ""; //where to put the mg level data once the highest level is determined. put in same level as coarse. this location keeps on getting overwritten every time mg_levels_n is encountered

//Call the "Tex" function which populates an array with TeX to be used instead of images
//var texMatrices = tex(maxMatricies) //unfortunately, cannot use anymore

//GetAndDisplayDirectory: modified from PETSc.getAndDisplayDirectory
//------------------------------------------------------------------
SAWsGetAndDisplayDirectory = function(names,divEntry){
    jQuery(divEntry).html(""); //clears divEntry
    SAWs.getDirectory(names,SAWsDisplayDirectory,divEntry);
}

//DisplayDirectory: modified from PETSc.displayDirectory
//------------------------------------------------------
SAWsDisplayDirectory = function(sub,divEntry)
{
    globaldirectory[divEntry] = sub;
    //alert("2. DisplayDirectory: sub="+sub+"; divEntry="+divEntry);
    if (sub.directories.SAWs_ROOT_DIRECTORY.variables.hasOwnProperty("__Block") && (sub.directories.SAWs_ROOT_DIRECTORY.variables.__Block.data[0] == "true")) {//this function is nearly always called
        SAWs.updateDirectoryFromDisplay(divEntry);
        sub.directories.SAWs_ROOT_DIRECTORY.variables.__Block.data = ["false"];
        SAWs.postDirectory(sub);
        jQuery(divEntry).html("");//empty divEntry
        window.setTimeout(SAWsGetAndDisplayDirectory,500,null,divEntry);//calls SAWsGetAndDisplayDirectory(null, divEntry) after 500ms
    }

    if (sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables._title.data == "Preconditioner (PC) options") {
        var SAWs_pcVal = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables["-pc_type"].data[0];
        var SAWs_alternatives = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables["-pc_type"].alternatives;
        var SAWs_prefix = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables["prefix"].data[0];

        if (SAWs_prefix == "(null)")//null on first pc I believe (because first pc has no prefix)
            SAWs_prefix = ""; //"(null)" fails populatePcList(), don't know why???

        if (typeof $("#pcList-1"+SAWs_prefix+"text").attr("title") == "undefined" && SAWs_prefix.indexOf("est")==-1) {//it doesn't exist already and doesn't contain 'est'
            $("#o-1").append("<br><b style='margin-left:20px;' title=\"Preconditioner\" id=\"pcList-1"+SAWs_prefix+"text\">-"+SAWs_prefix+"pc_type &nbsp; &nbsp;</b><select class=\"pcLists\" id=\"pcList-1"+SAWs_prefix+"\"></select>");
            populatePcList("pcList-1"+SAWs_prefix,SAWs_alternatives,SAWs_pcVal);

            //parse through prefix...

            //first determine what fieldsplit level we are working with, then determine what endtag we are working with

            /*var fieldsplit="0";
            while(SAWs_prefix.indexOf("fieldsplit_")!=-1) {
                //find index of next keyword (pc, ksp, sub, smoothing, coarse)

            }*/

            //new spot in sawsInfo if needed, etc WILL ADDRESS ALL OF THIS LATER. FOR NOW, ONLY WORKS WITH NO FIELDSPLITS
            //var index=getSawsIndex(sawsCurrentFieldsplit);

            var index=0;//hard code for now
            sawsInfo[index].id="0";//hard code for now
            var endtag="";
            while(SAWs_prefix!="") {//parse the entire prefix
                var indexFirstUnderscore=SAWs_prefix.indexOf("_");
                var chunk=SAWs_prefix.substring(0,indexFirstUnderscore);//dont include the underscore

                if(chunk=="mg") {//mg_
                    indexFirstUnderscore=SAWs_prefix.indexOf("_",3); //this will actually be the index of the second underscore now since mg prefix has underscore in itself
                    chunk=SAWs_prefix.substring(0,indexFirstUnderscore);//updated chunk
                }

                if(chunk=="mg_levels") {//need to include yet another underscore
                    indexFirstUnderscore=SAWs_prefix.indexOf("_",10); //this will actually be the index of the third underscore
                    chunk=SAWs_prefix.substring(0,indexFirstUnderscore);//updated chunk
                }

                SAWs_prefix=SAWs_prefix.substring(indexFirstUnderscore+1, SAWs_prefix.length);//dont include the underscore

                if(chunk=="mg_coarse" && SAWs_prefix=="")//new mg coarse
                    mgLevelLocation=endtag+"0";

                if(chunk=="ksp" || chunk=="sub" || chunk=="mg_coarse" || chunk=="redundant")
                    endtag+="0";
                else if(chunk=="mg_levels_1")
                    endtag+="1";//actually should add a variable amount
                else if(chunk=="mg_levels_2")
                    endtag+="2";
                else if(chunk=="mg_levels_3")
                    endtag+="3";

                if(chunk=="mg_levels_1" && SAWs_prefix=="")//new mg levels. it's okay to assume memory was already allocated b/c levels is after coarse
                    sawsInfo[index].data[getSawsDataIndex(index, mgLevelLocation)].mg_levels=2;
                if(chunk=="mg_levels_2" && SAWs_prefix=="")
                    sawsInfo[index].data[getSawsDataIndex(index, mgLevelLocation)].mg_levels=3;
                if(chunk=="mg_levels_3" && SAWs_prefix=="")
                    sawsInfo[index].data[getSawsDataIndex(index, mgLevelLocation)].mg_levels=4;
            }

            if(typeof sawsInfo[index] == undefined)
                sawsInfo[index]=new Object();
            if(sawsInfo[index].data == undefined)//allocate new mem if needed
                sawsInfo[index].data=new Array();
            //search if it has already been created
            if(getSawsDataIndex(index,endtag) == -1) {//doesn't already exist so allocate new memory
                sawsInfo[index].data[sawsDataWriteCounter]=new Object();
                sawsInfo[index].data[sawsDataWriteCounter].endtag=endtag;
                sawsDataWriteCounter++;
            }
            var index2=getSawsDataIndex(index,endtag);
            sawsInfo[index].data[index2].pc=SAWs_pcVal;

            if (SAWs_pcVal == 'bjacobi') {//some extra data for bjacobi
                //saws does bjacobi_blocks differently than we do. we put bjacoi_blocks as a different endtag than bjacobi dropdown (lower level) but saws puts them on the same level so we need to add a "0" to the endtag
                endtag=endtag+"0";
                if(getSawsDataIndex(index,endtag)==-1) {//need to allocate new memory
                    sawsInfo[index].data[sawsDataWriteCounter]=new Object();
                    sawsInfo[index].data[sawsDataWriteCounter].endtag=endtag;
                    sawsDataWriteCounter++;
                }
                sawsInfo[index].data[getSawsDataIndex(index,endtag)].bjacobi_blocks = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables["-pc_bjacobi_blocks"].data[0];
            }

        }

        // SAWs_pcVal == 'mg' NEED TO GET MG LEVELS SOMEHOW

    } else if (sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables._title.data == "Krylov Method (KSP) options") {
        var SAWs_kspVal       = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables["-ksp_type"].data[0];
        var SAWs_alternatives = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables["-ksp_type"].alternatives;
        var SAWs_prefix       = sub.directories.SAWs_ROOT_DIRECTORY.directories.PETSc.directories.Options.variables.prefix.data[0];

        if (SAWs_prefix == "(null)")
            SAWs_prefix = "";

        if (typeof $("#kspList-1"+SAWs_prefix+"text").attr("title") == "undefined" && SAWs_prefix.indexOf("est")==-1) {//it doesn't exist already and doesn't contain 'est'
            $("#o-1").append("<br><b style='margin-left:20px;' title=\"Krylov method\" id=\"kspList-1"+SAWs_prefix+"text\">-"+SAWs_prefix+"ksp_type &nbsp;</b><select class=\"kspLists\" id=\"kspList-1"+SAWs_prefix+"\"></select>");//giving an html element a title creates a tooltip
            populateKspList("kspList-1"+SAWs_prefix,SAWs_alternatives,SAWs_kspVal);

            //parse through prefix...
            //first determine what fieldsplit level we are working with, then determine what endtag we are working with

            /*var fieldsplit="0";
            while(SAWs_prefix.indexOf("fieldsplit_")!=-1) {
                //find index of next keyword (pc, ksp, sub, smoothing, coarse)

            }*/

            //new spot in sawsInfo if needed, etc WILL ADDRESS ALL OF THIS LATER. FOR NOW, ONLY WORKS WITH NO FIELDSPLITS
            //var index=getSawsIndex(sawsCurrentFieldsplit);

            var index=0;//hard code for now
            sawsInfo[index].id="0";//hard code for now
            var endtag="";

            while(SAWs_prefix!="") {//parse the entire prefix
                var indexFirstUnderscore=SAWs_prefix.indexOf("_");
                var chunk=SAWs_prefix.substring(0,indexFirstUnderscore);//dont include the underscore

                if(chunk=="mg") {//mg_
                    indexFirstUnderscore=SAWs_prefix.indexOf("_",3); //this will actually be the index of the second underscore now since mg prefix has underscore in itself
                    chunk=SAWs_prefix.substring(0,indexFirstUnderscore);//updated chunk
                }

                if(chunk=="mg_levels") {//need to include yet another underscore
                    indexFirstUnderscore=SAWs_prefix.indexOf("_",10); //this will actually be the index of the third underscore
                    chunk=SAWs_prefix.substring(0,indexFirstUnderscore);//updated chunk
                }

                SAWs_prefix=SAWs_prefix.substring(indexFirstUnderscore+1, SAWs_prefix.length);//dont include the underscore
                if(chunk=="ksp" || chunk=="sub" || chunk=="mg_coarse" || chunk=="redundant")
                    endtag+="0";
                else if(chunk=="mg_levels_1")
                    endtag+="1";//actually should add a variable amount
                else if(chunk=="mg_levels_2")
                    endtag+="2";
                else if(chunk=="mg_levels_3")
                    endtag+="3";
            }

            if(typeof sawsInfo[index] == undefined)
                sawsInfo[index]=new Object();
            if(sawsInfo[index].data == undefined)//allocate new mem if needed
                sawsInfo[index].data=new Array();
            //search if it has already been created
            if(getSawsDataIndex(index,endtag) == -1) {//doesn't already exist so allocate new memory
                sawsInfo[index].data[sawsDataWriteCounter]=new Object();
                sawsInfo[index].data[sawsDataWriteCounter].endtag=endtag;
                sawsDataWriteCounter++;
            }
            var index2=getSawsDataIndex(index,endtag);
            sawsInfo[index].data[index2].ksp=SAWs_kspVal;
        }
    }

    SAWs.displayDirectoryRecursive(sub.directories,divEntry,0,"");//this function is not in SAWs API ?
}

// 1. When pcoptions.html is loaded ...
//-------------------------------------
HandlePCOptions = function(){
    alert('2. in HandlePCOptions()...');

    //reset the form
    formSet(currentAsk);

    //hide at first
    $("#fieldsplitBlocks_text").hide();
    $("#fieldsplitBlocks").hide();

    //must define these parameters before setting default pcVal, see populatePcList() and listLogic.js!
    matInfo[-1] = {
        posdef:  false,
        symm:    false,
        logstruc:false,
        blocks:  0,
        matLevel:0,
        id:      "0"
    }

    matInfo[0] = {
        posdef:  false,
        symm:    false,
        logstruc:false,
        blocks:  0,
        matLevel:0,
        id:      "0"
    }

    sawsInfo[0] = {
        id: "0"
    }
    sawsInfoWriteCounter++;

    //create div 'o-1' for displaying SAWs options
    $("#divPc").append("<div id=\"o-1\"> </div>");

    // get and display SAWs options
    SAWsGetAndDisplayDirectory("","#variablesInfo");//this #variablesInfo variable only appears here

    //When "Continue" button is clicked ...
    //----------------------------------------
    $("#continueButton").click(function(){

	//matrixLevel is how many matrices deep the data is. 0 is the overall matrix,
        var matrixLevel = currentAsk.length-1;//minus one because A0 is length 1 but level 0
        var fieldsplitBlocks = $("#fieldsplitBlocks").val();

        if (!document.getElementById("logstruc").checked)
            fieldsplitBlocks=0;//sometimes will be left over garbage value from previous submits

        //we don't have to worry about possibility of symmetric and not posdef because when symmetric is unchecked, it not only hides posdef but also removes the checkmark if there was one

	//Write the form data to matInfo
	matInfo[matInfoWriteCounter] = {
            posdef:  document.getElementById("posdef").checked,
            symm:    document.getElementById("symm").checked,
            logstruc:document.getElementById("logstruc").checked,
            blocks:  fieldsplitBlocks,
            matLevel:matrixLevel,
            id:      currentAsk
	}

        //increment write counter immediately after data is written
        matInfoWriteCounter++;

        //append to table of two columns holding A and oCmdOptions in each column (should now be changed to simply cmdOptions)
        //tooltip contains all information previously in big letter format (e.g posdef, symm, logstruc, etc)
        var indentation = matrixLevel*30; //according to the length of currentAsk (aka matrix level), add margins of 30 pixels accordingly
        $("#oContainer").append("<tr id='row"+currentAsk+"'> <td> <div style=\"margin-left:"+indentation+"px;\" id=\"A"+ currentAsk + "\"> </div></td> <td> <div id=\"oCmdOptions" + currentAsk + "\"></div> </td> </tr>");

        //Create drop-down lists. '&nbsp;' indicates a space
        $("#A" + currentAsk).append("<br><b id='matrixText"+currentAsk+"'>A" + currentAsk + " (Symm:"+matInfo[matInfoWriteCounter-1].symm+" Posdef:"+matInfo[matInfoWriteCounter-1].posdef+" Logstruc:"+matInfo[matInfoWriteCounter-1].logstruc +")</b>");
	$("#A" + currentAsk).append("<br><b>KSP &nbsp;</b><select class=\"kspLists\" id=\"kspList" + currentAsk +"\"></select>");
	$("#A" + currentAsk).append("<br><b>PC &nbsp; &nbsp;</b><select class=\"pcLists\" id=\"pcList" + currentAsk +"\"></select>");

        if(matInfo[matInfoWriteCounter-1].logstruc) {//if fieldsplit, need to add the fieldsplit type and fieldsplit blocks
            var newDiv = generateDivName("",currentAsk,"fieldsplit");//this div contains the two fieldsplit dropdown menus. as long as first param doesn't contain "_", it will generate assuming it is directly under an A-div which it is
	    var endtag = newDiv.substring(newDiv.lastIndexOf('_'), newDiv.length);
	    $("#A"+currentAsk).append("<div id=\""+newDiv+"\" style='margin-left:"+30+"px;'></div>");//append to the A-div that we just added to the table
            var myendtag = endtag+"0";
	    $("#"+newDiv).append("<b>Fieldsplit Type &nbsp;&nbsp;</b><select class=\"fieldsplitList\" id=\"fieldsplitList" + currentAsk +myendtag+"\"></select>");
            $("#"+newDiv).append("<br><b>Fieldsplit Blocks </b><input type='text' id='fieldsplitBlocks"+currentAsk+myendtag+"\' value='2' maxlength='2' class='fieldsplitBlocks'>");//note that class is fieldsplitBlocks NOT fieldsplitBlocksInput
            populateFieldsplitList("fieldsplitList"+currentAsk+myendtag);
        }

	//populate the kspList and pclist with default options
        if (getSawsIndex(currentAsk) != -1) { //use SAWs options if they exist for this matrix
            var sawsIndex=getSawsIndex(currentAsk);

            var SAWs_kspVal = sawsInfo[sawsIndex].data[getSawsDataIndex(sawsIndex,"")].ksp;//want the ksp where endtag=""
            //SAWs_alternatives ???
            populateKspList("kspList"+currentAsk,null,SAWs_kspVal);

            var SAWs_pcVal = sawsInfo[sawsIndex].data[getSawsDataIndex(sawsIndex,"")].pc;//want the pc where endtag=""
            //SAWs_alternatives ???
	    populatePcList("pcList"+currentAsk,null,SAWs_pcVal);
        } else {//else, use default values
            populateKspList("kspList"+currentAsk,null,"null");
            populatePcList("pcList"+currentAsk,null,"null");
        }

        //manually trigger pclist once because additional options, e.g., detailed info may need to be added
        if($("#pcList"+currentAsk).val()!="fieldsplit")//but DON'T trigger change on fieldsplit because that would append the required A divs twice
	    $("#pcList"+currentAsk).trigger("change");

        currentAsk = matTreeGetNextNode(currentAsk);
        //alert("new current ask:"+currentAsk);

        formSet(currentAsk); //reset the form

	//Tell mathJax to re compile the tex data
	//MathJax.Hub.Queue(["Typeset",MathJax.Hub]); //unfortunately, cannot use this anymore
        //alert('endof continueButton');
    });
    alert('exit HandlePCOptions()...');
}


//  This function is run when the page is first visited
//-----------------------------------------------------
$(document).ready(function(){
    alert('0. start run page...');
    $(function() { //needed for jqueryUI tool tip to override native javascript tooltip
        $(document).tooltip();
    });

    //When the button "Logically Block Structured" is clicked...
    //----------------------------------------------------------
    $("#logstruc").change(function(){
        if (document.getElementById("logstruc").checked) {
            $("#fieldsplitBlocks_text").show();
            $("#fieldsplitBlocks").show();
            //populatePcList("pcList-1",null,"fieldsplit");//HAD THIS ORIGINALLY
        } else {
            $("#fieldsplitBlocks_text").hide();
            $("#fieldsplitBlocks").hide();
        }
    });

    //this is ONLY for the input box in the beginning form. NOT the inputs in the A divs (those have class='fieldsplitBlocks')
    //-------------------------------------------------------------------------------------------------------------------------
    $(document).on("keyup", '.fieldsplitBlocksInput', function() {//alerts user with a tooltip when an invalid input is provided
        //alert('when this is called?'); ???
        if ($(this).val().match(/[^0-9]/) || $(this).val()==0 || $(this).val()==1) {//problem is that integer only bubble still displays when nothing is entered
	    $(this).attr("title","hello");//set a random title (this will be overwritten)
	    $(this).tooltip();//create a tooltip from jquery UI
	    $(this).tooltip({content: "At least 2 blocks!"});//edit displayed text
	    $(this).tooltip("open");//manually open once
        } else {
	    $(this).removeAttr("title");//remove title attribute
	    $(this).tooltip("destroy");
        }
    });

    //Only show positive definite if symmetric
    //----------------------------------------
    $("#symm").change(function(){
        if (document.getElementById("symm").checked) {
            $("#posdefRow").show();
        } else {
            $("#posdefRow").hide();
            $("#posdef").removeAttr("checked");
        }
    });

    //When "Cmd Options" button is clicked ... 
    //----------------------------------------
    var treeDetailed;
    $("#cmdOptionsButton").click(function(){
	$("#treeContainer").html("<div id='tree'> </div>");

	//get the options from the drop-down lists
        solverGetOptions(matInfo);

	//get the number of levels for the tree for scaling purposes
        var matLevelForTree=0;
        for(var i=0; i<matInfoWriteCounter; i++)
            if(matInfo[i].id!="-1" && matInfo[i].level>matLevelForTree)
                matLevelForTree=matInfo[i];
        matLevelForTree++;//appears to be 1 greater than the max

	//build the tree
        treeDetailed = false;//tree.js uses this variable to know what information to display
	buildTree(matInfo,matLevelForTree,treeDetailed);

        //show cmdOptions to the screen
        for (var i=0; i<matInfoWriteCounter; i++) {
	    if (matInfo[i].id=="-1")//possible junk value created by deletion of adiv
		continue;
	    $("#oCmdOptions" + matInfo[i].id).empty();
            $("#oCmdOptions" + matInfo[i].id).append("<br><br>" + matInfo[i].string);
            //MathJax.Hub.Queue(["Typeset",MathJax.Hub]); //Tell mathJax to re compile the tex data    
        }
    });

    $("#solverTreeButtonDetailed").click(function(){
	$("#treeContainer").html("<div id='tree'> </div>");

	//get the options from the drop-down lists
        solverGetOptions(matInfo);

	//get the number of levels for the tree for scaling purposes
        var matLevelForTree=0;
        for(var i=0; i<matInfoWriteCounter; i++)
            if(matInfo[i].id!="-1" && matInfo[i].level>matLevelForTree)
                matLevelForTree=matInfo[i];
        matLevelForTree++;//appears to be 1 greater than the max

	//build the tree
        treeDetailed = true;//tree.js uses this variable
	buildTree(matInfo,matLevelForTree,treeDetailed);

        //show cmdOptions to the screen
        for (var i=0; i<matInfoWriteCounter; i++) {
	    if (matInfo[i].id=="-1")//possible junk value created by deletion of adiv
		continue;
	    $("#oCmdOptions" + matInfo[i].id).empty();
            $("#oCmdOptions" + matInfo[i].id).append("<br><br>" + matInfo[i].string);
        }
    });

    $("#clearOutput").click(function(){
	for(var i=0; i<matInfoWriteCounter; i++)
	    $("#oCmdOptions"+matInfo[i].id).empty();//if matInfo has deleted A-divs, its still okay because id will be "-1" and nothing will be changed
    });

    $("#clearTree").click(function(){
        $("#tree").remove();
    });

    alert('1. call HandlePCOptions()...');
    HandlePCOptions(); //big function is called here???
});

//-----------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------

/*
  formSet - Set Form (hide form if needed)
  input:
    currentAsk
  ouput:
    Form asking questions for currentAsk
*/
function formSet(current)//-1 input for current means that program has finished
{
    if (current=="-1") {//finished asking
        $("#questions").hide();
        finishedAsking=true;
        return;
    }

    $("#currentAskText").html("<b id='currentAskText'>Currently Asking for Matrix A"+current+"</b>");
    $("#posdefRow").hide();
    $("#fieldsplitBlocks").hide();
    $("#fieldsplitBlocks_text").hide();
    $("#symm").removeAttr("checked");
    $("#posdef").removeAttr("checked");
    $("#logstruc").removeAttr("checked");

    if(current == "0")//special case for first node since no defaults were set yet
        return;

    //fill in the form according to defaults set by matTreeGetNextNode
    if (matInfo[getMatIndex(current)].symm) {//if symmetric
        $("#posdefRow").show();
        $("#symm").prop("checked", "true");
    }
    //if posdef, fill in bubble
    if (matInfo[getMatIndex(current)].posdef) {
        $("#posdef").prop("checked", "true");
    }
}

/*
  pcGetDetailedInfo - get detailed information from the pclists
  input:
    pcListID
    prefix - prefix of the options in the solverTree
    recursionCounter - id of A-div we are currently working in
  output:
    matInfo.string
    matInfo.stringshort
*/
function pcGetDetailedInfo(pcListID, prefix,recursionCounter,matInfo) 
{
    var pcSelectedValue=$("#"+pcListID).val();
    var info      = "";
    var infoshort = "";
    var endtag,myendtag;

    if (pcListID.indexOf("_") == -1) {//dealing with original pcList in the oDiv (not generated for suboptions)
	endtag = "_"; // o-th solver level
    } else {
        var loc = pcListID.indexOf("_");
        endtag = pcListID.substring(loc); // endtag of input pcListID, eg. _, _0, _00, _10
    }
    //alert("pcGetDetailedInfo: pcListID="+pcListID+"; pcSelectedValue= "+pcSelectedValue+"; endtag= "+endtag);

    switch(pcSelectedValue) {
    case "mg" :
        myendtag = endtag+"0";
	var mgType       = $("#mgList" + recursionCounter + myendtag).val();
        var mgLevels     = $("#mglevels" + recursionCounter + myendtag).val();
	info   += "<br />"+prefix+"pc_mg_type " + mgType + "<br />"+prefix+"pc_mg_levels " + mgLevels;
        prefix += "mg_";
        var index=getMatIndex(recursionCounter);
        matInfo[index].string      += info;
        matInfo[index].stringshort += infoshort;

        var smoothingKSP;
        var smoothingPC;
        var coarseKSP;
        var coarsePC;
        var level;

        //is a composite pc so there will be a div in the next position
        var generatedDiv="";
        generatedDiv = $("#"+pcListID).next().get(0).id; //this will be a div
        //alert("generatedDiv "+generatedDiv+"; children().length="+$("#"+generatedDiv).children().length);
        level = mgLevels-1;
        for (var i=0; i<$("#"+generatedDiv).children().length; i++) { //loop over all pcLists under this Div
	    var childID = $("#"+generatedDiv).children().get(i).id;
	    if ($("#"+childID).is(".pcLists")) {//has more pc lists that need to be taken care of recursively
                info      = "";
                infoshort = "";
                if (level) {
		    if(level<10)//still using numbers
			myendtag = endtag+level;
		    else
			myendtag = endtag+'abcdefghijklmnopqrstuvwxyz'.charAt(level-10);//add the correct char

                    smoothingKSP = $("#kspList" + recursionCounter + myendtag).val();
	            smoothingPC  = $("#pcList" + recursionCounter + myendtag).val();
                    info   += "<br />"+prefix+"levels_"+level+"_ksp_type "+smoothingKSP+"<br />"+prefix+"levels_"+level+"_pc_type "+smoothingPC;
                    infoshort += "<br />Level "+level+" -- KSP: "+smoothingKSP+"; PC: "+smoothingPC;
                    var myprefix = prefix+"levels_"+level+"_";
                } else if (level == 0) {
                    myendtag = endtag+"0";
                    coarseKSP    = $("#kspList" + recursionCounter + myendtag).val();
	            coarsePC     = $("#pcList" + recursionCounter + myendtag).val();
                    info   += "<br />"+prefix+"coarse_ksp_type "+coarseKSP+"<br />"+prefix+"coarse_pc_type "+coarsePC;
                    infoshort += "<br />Coarse Grid -- KSP: "+coarseKSP+"; PC: "+coarsePC;
                    var myprefix = prefix+"coarse_";
                } else {
                    alert("Error: mg level cannot be "+level);
                }
                var index=getMatIndex(recursionCounter);
                matInfo[index].string      += info;
                matInfo[index].stringshort += infoshort;

                pcGetDetailedInfo(childID,myprefix,recursionCounter,matInfo);
                level--;
	    }
        }
        return "";
        break;

    case "redundant" :
        endtag += "0"; // move to next solver level
	var redundantNumber = $("#redundantNumber" + recursionCounter + endtag).val();
	var redundantKSP    = $("#kspList" + recursionCounter + endtag).val();
	var redundantPC     = $("#pcList" + recursionCounter + endtag).val();

        info += "<br />"+prefix+"pc_redundant_number " + redundantNumber;
        prefix += "redundant_";
	info += "<br />"+prefix+"ksp_type " + redundantKSP +"<br />"+prefix+"pc_type " + redundantPC;
        infoshort += "<br />PCredundant -- KSP: " + redundantKSP +"; PC: " + redundantPC;
        break;

    case "bjacobi" :
        endtag += "0"; // move to next solver level
	var bjacobiBlocks = $("#bjacobiBlocks" + recursionCounter + endtag).val();
	var bjacobiKSP    = $("#kspList" + recursionCounter + endtag).val();
	var bjacobiPC     = $("#pcList" + recursionCounter + endtag).val();
        info   += "<br />"+prefix+"pc_bjacobi_blocks "+bjacobiBlocks; // option for previous solver level 
        prefix += "sub_";
        info   += "<br />"+prefix+"ksp_type "+bjacobiKSP+"<br />"+prefix+"pc_type "+bjacobiPC;
        infoshort  += "<br /> PCbjacobi -- KSP: "+bjacobiKSP+"; PC: "+bjacobiPC;
        break;

    case "asm" :
        endtag += "0"; // move to next solver level
        var asmBlocks  = $("#asmBlocks" + recursionCounter + endtag).val();
        var asmOverlap = $("#asmOverlap" + recursionCounter + endtag).val();
	var asmKSP     = $("#kspList" + recursionCounter + endtag).val();
	var asmPC      = $("#pcList" + recursionCounter + endtag).val();
	info   +=  "<br />"+prefix+"pc_asm_blocks  " + asmBlocks + " "+prefix+"pc_asm_overlap "+ asmOverlap; 
        prefix += "sub_";
        info   += "<br />"+prefix+"ksp_type " + asmKSP +" "+prefix+"pc_type " + asmPC;
        infoshort += "<br />PCasm -- KSP: " + asmKSP +"; PC: " + asmPC;
        break;

    case "ksp" :
        endtag += "0"; // move to next solver level
	var kspKSP = $("#kspList" + recursionCounter + endtag).val();
	var kspPC  = $("#pcList" + recursionCounter + endtag).val();
        prefix += "ksp_";
        info   += "<br />"+prefix+"ksp_type " + kspKSP + " "+prefix+"pc_type " + kspPC;
        infoshort += "<br />PCksp -- KSP: " + kspKSP + "; PC: " + kspPC;
        break;

    default :
    }

    if  (info.length == 0) return ""; //is not a composite pc. no extra info needs to be added
    var index=getMatIndex(recursionCounter);
    matInfo[index].string      += info;
    matInfo[index].stringshort += infoshort;

    //is a composite pc so there will be a div in the next position
    var generatedDiv="";
    generatedDiv = $("#"+pcListID).next().get(0).id; //this will be a div, eg. mg0_, bjacobi1_
    //alert("generatedDiv "+generatedDiv);
    for (var i=0; i<$("#"+generatedDiv).children().length; i++) { //loop over all pcLists under this Div
	var childID = $("#"+generatedDiv).children().get(i).id;
	if ($("#"+childID).is(".pcLists")) {//has more pc lists that need to be taken care of recursively
            pcGetDetailedInfo(childID,prefix,recursionCounter,matInfo);
	}
    }
}

/*
  matTreeGetNextNode - uses matInfo to find and return the id of the next node to ask about SKIP ANY CHILDREN FROM NON-LOG STRUC PARENT
  input: 
    currentAsk
  output: 
    id of the next node that should be asked
*/
function matTreeGetNextNode(current)
{
    //important to remember that writeCounter is already pointing at an empty space at this point. this method also initializes the next object if needed.
    if (current=="0" && askedA0)
        return -1;//sort of base case. this only occurs when the tree has completely finished

    if (current=="0")
        askedA0 = true;

    var parentID  = current.substring(0,current.length-1);//simply knock off the last digit of the id
    var lastDigit = current.charAt(current.length-1);
    lastDigit     = parseInt(lastDigit);

    var currentBlocks = matInfo[getMatIndex(current)].blocks;
    var possibleChild = current+""+(currentBlocks-1);

    //case 1: current node needs more child nodes

    if (matInfo[getMatIndex(current)].logstruc && currentBlocks!=0 && getMatIndex(possibleChild)==-1) {//CHECK TO MAKE SURE CHILDREN DON'T ALREADY EXIST
        //alert("needs more children");
        matInfo[matInfoWriteCounter]        = new Object();
        matInfo[matInfoWriteCounter].symm   = matInfo[getMatIndex(current)].symm;//set defaults for the new node
        matInfo[matInfoWriteCounter].posdef = matInfo[getMatIndex(current)].posdef;

        return current+"0";//move onto first child
    }

    //case 2: current node's child nodes completed. move on to sister nodes if any

    if (current!="0" && lastDigit+1 < matInfo[getMatIndex(parentID)].blocks) {
        //alert("needs more sister nodes");
        matInfo[matInfoWriteCounter]        = new Object();
        matInfo[matInfoWriteCounter].symm   = matInfo[getMatIndex(current)].symm;//set defaults for the new node
        matInfo[matInfoWriteCounter].posdef = matInfo[getMatIndex(current)].posdef;
        var newEnding                       = parseInt(lastDigit)+1;

        return ""+parentID+newEnding;
    }

    if (parentID=="")//only happens when there is only one A matrix
        return -1;

    //case 3: recursive case. both current node's child nodes and sister nodes completed. recursive search starting on parent again
    return matTreeGetNextNode(parentID);
}

/*
  solverGetOptions - get the options from the drop-down lists
  input:
    matInfo
  output:
    matInfo[].string stores collected solver options
    matInfo[].stringshort
*/
function solverGetOptions(matInfo)
{
    var prefix,kspSelectedValue,pcSelectedValue,level;

    for (var i = 0; i<matInfoWriteCounter; i++) {
	if (typeof matInfo[i] != 'undefined' && matInfo[i].id!="-1") {
	    //get the ksp and pc options at the topest solver-level
	    kspSelectedValue = $("#kspList" + matInfo[i].id).val();
	    pcSelectedValue  = $("#pcList" + matInfo[i].id).val();

            //get prefix
            prefix = "-";
            // for pc=fieldsplit
            for (level=1; level<=matInfo[i].matLevel; level++) {
                if (level == matInfo[i].matLevel) {
                    prefix += "fieldsplit_A"+matInfo[i].id+"_"; // matInfo[i].id
                } else {
                    var parent = matInfo[i].id.substring(0,matInfo[i].id.length-1);//take everything except last char
                    var parentLevel = parent.length-1;//by definition. because A0 is length 1 but level 0
                    while (level < parentLevel) {
                        parent = parent.substring(0,parent.length-1);//take everything except last char
                        parentLevel = parent.length-1;
                    }
                    prefix += "fieldsplit_A"+parent+"_";
                }
            }

	    //together, with the name, make a full string for printing
            matInfo[i].string = ("Matrix A" + matInfo[i].id + "<br /> "+prefix+"ksp_type " + kspSelectedValue + "<br />"+prefix+"pc_type " + pcSelectedValue);
            matInfo[i].stringshort = ("Matrix A" + matInfo[i].id + "<br /> KSP: " + kspSelectedValue + "; PC: " + pcSelectedValue);

            // for composite pc, get additional info from the rest of pcLists
            pcGetDetailedInfo("pcList"+ matInfo[i].id,prefix,matInfo[i].id,matInfo);
	}
    }
}

/*
  getMatIndex - 
  input: 
    desired id in string format. (for example, "01001")
  output: 
    index in matInfo where information on that id is located
*/
function getMatIndex(id)
{
    for (var i=0; i<matInfoWriteCounter; i++) {
        if (matInfo[i].id == id)
            return i;//return index where information is located.
    }
    return -1;//invalid id.
}

/*
  getSawsIndex -
  input:
    desired id in string format. (for example, "01001")
  output:
    index in sawsInfo where information on that id is located
*/
function getSawsIndex(id)
{
    for(var i=0; i<sawsInfoWriteCounter; i++) {
        if(sawsInfo[i].id == id)
            return i;//return index where information is located
    }
    return -1;//invalid id;
}

function getSawsDataIndex(id, endtag)//id is the adiv we are working with. endtag is the id of the data we are looking for
{
    for(var i=0; i<sawsDataWriteCounter; i++) {
        if(sawsInfo[id].data[i].endtag == endtag)
            return i;//return index where information is located
    }
    return -1;//invalid id;
}