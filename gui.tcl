package require Tk
catch {package require tile}
if [catch {ttk::setTheme aqua}] {
	if [catch {ttk::setTheme tilegtk}] {
		catch {ttk::setTheme tileqt}
	}
}
catch {namespace import -force ttk::*}
source scrollable.tcl

proc clearScrollableThing {widget} {
	foreach item [grid slaves ${widget}.frame] {
		grid forget $item
		destroy $item
	}
}

proc drawPackageList {destination data} {
	global highlightedrow
	global selectedPackages
	set i 0
	set highlightedrow 0
	foreach {item} $data {
		array set temp $item

		canvas ${destination}.frame.row$i -highlightbackground #abc -highlightthickness 0
		grid ${destination}.frame.row$i -pady 2 -sticky nwe -row $i

		# When reading the database, only use its value if we haven't already picked one
		# This fixes the problem where anytime the list is reread it would clobber your selection changes.
		if {![info exists selectedPackages($temp(title))]} {
			if {$temp(status) != "not installed"} {
				set selectedPackages($temp(title)) 1
			} else {
				set selectedPackages($temp(title)) 0
			}
		}

		set cb [checkbutton ${destination}.frame.row${i}.check -variable selectedPackages($temp(title))]
		set icon [canvas $destination.frame.row$i.icon -height 24 -width 24 -background blue]
		set name [label ${destination}.frame.row$i.desc -text $temp(title) -anchor w -font TkHeadingFont]
		set desc [label ${destination}.frame.row$i.longer -text $temp(descText) -anchor w]

		# Should get longer info from search eventually
		set handler "set currentPackage(title) {$temp(title)}
								 set currentPackage(caption) {$temp(descText)}
								 set currentPackage(longText) {$temp(longDesc)}
								 ${destination}.frame.row\$highlightedrow configure -highlightthickness 0
								 set highlightedrow $i
								 ${destination}.frame.row$i configure -highlightthickness 2
								 "
		bind ${destination}.frame.row$i <ButtonPress-1> $handler
		bind $name <ButtonPress-1> $handler
		bind $icon <ButtonPress-1> $handler
		bind $desc <ButtonPress-1> $handler

		grid $cb -column 0 -rowspan 2 -padx 5 -row $i
		grid $icon -column 1 -rowspan 2 -padx 5 -row $i
		grid $name -column 2 -padx 5 -pady 2 -sticky nwe -row $i
		grid $desc -column 2 -padx 5 -pady 2 -sticky nwe -row [expr {1+$i}]

		grid columnconfigure ${destination}.frame.row$i 2 -weight 1

		incr i 2
	}
}

proc lineTrim {words} {
	upvar $words temp
	set temp [string trim $temp]
	regsub {\s*\n\s*} $temp {\n} temp
}

proc getPackList {text category} {
	if {$category !=""} {
		set category "-i$category"
	}
	set rawOutput [exec search/search -v $category $text]
	set output [split [string map [list "\n\n" \0] $rawOutput] \0]

	set packList [list]
	foreach pack $output {
		array set temp [list]

		#This part runs all of the parses, and if any of them fail the error part runs
		if {!(
			[regexp {Package: ([^\n]*)\n} $pack mat temp(title)] && 
			[regexp {Status: ([^\n]*)\n} $pack mat temp(status)] &&
			[regexp {Description: ([^\n]*)\n(.*)} $pack mat temp(descText) temp(longDesc)]
			)
		} {
			# ERROR
			puts "Package parse error: \n$rawOutput"
			return [list]
		}
		lineTrim temp(longDesc)

		lappend packList [array get temp]
	}
	return $packList
}

proc filter {listWidget text category} {
	clearScrollableThing $listWidget
	drawPackageList $listWidget [getPackList $text $category]
}

proc categoryUpdate {path} {
	global filterCategory
	global filterCategoryDisplayNameMap
	# Get the data from the comboBox
	# Convert from the view value to the data value
	set filterCategory $filterCategoryDisplayNameMap([$path get])
	# If the value is "All"
	if {$filterCategory == ""} {
		# Set the value to be "Category"
		$path set "Category"
		$path selection clear
	}
	# Filter
	getDataAndFilter
}

proc getDataAndFilter {} {
	global canvas 
	global searchQuery
	global filterCategory
	filter $canvas $searchQuery $filterCategory
}

# Get the main scrollable canvas
set canvas [scrollableThing .can]
$canvas configure -yscrollcommand {.yscroll set}
scrollbar .yscroll -orient vertical -command {$canvas yview}

# Get scrollable view area
set viewarea [scrollableThing .viewarea]
$viewarea configure -yscrollcommand {.viewyscroll set}
scrollbar .viewyscroll -orient vertical -command {$viewarea yview}

# Make the search area.
set searchArea [frame .searchArea]
set searchBar [entry ${searchArea}.bar -width 20 -textvariable searchQuery]
set searchButton [button ${searchArea}.button -text "Search" -command getDataAndFilter]
bind $searchBar <Return> "$searchButton invoke"
grid $searchBar $searchButton
grid $searchBar -sticky ew
grid $searchButton -sticky e
grid columnconfigure $searchArea 0 -weight 1

# Grid the search box
grid $searchArea -sticky ew

# Make the category box
set categoryArea [frame .categoryArea]
# Set the map that maps from display name to data name
array set filterCategoryDisplayNameMap [list Action actiongame Adventure adventuregame Arcade arcadegame "Board Game" boardgame "Blocks Game" blocksgame "Card Game" cardgame "Kids" kidsgame "Logic" logicgame "Role Playing" roleplaying Simulation simulation Sports sportsgame Strategy strategy]
# Then get the sorted list of categories, with "All" at the start
set categoryList [concat All [lsort [array names filterCategoryDisplayNameMap]]]
# Add the mapping form "All" to the filter
set filterCategoryDisplayNameMap(All) ""
# Find the required width of the combobox
set categoryMaxWidth 0
foreach item [concat "Category" $categoryList] {
	if {[string length $item] > $categoryMaxWidth} {
		set categoryMaxWidth [string length $item]
	}
}
set categoryCombo [ttk::combobox ${categoryArea}.categoryCombo -value $categoryList -width $categoryMaxWidth]
# Set the categoryCombo boxes value to Category
$categoryCombo set "Category"
bind $categoryCombo <<ComboboxSelected>> {categoryUpdate %W}
grid $categoryCombo

# Grid the category area
grid $categoryArea -sticky ew

# Grid the canvas and scrollbar
grid $canvas .yscroll
grid $canvas -sticky news
grid .yscroll -sticky ns

# Grid the viewarea and scrollbar
grid $viewarea .viewyscroll
grid $viewarea -sticky news
grid .viewyscroll -sticky ns

# Make grid fill window
grid rowconfigure . 2 -weight 1
grid rowconfigure . 3 -weight 1
grid columnconfigure . 0 -weight 1

# And make rows fill canvas
grid columnconfigure ${canvas}.frame 0 -weight 1

# Add label to viewarea
set tabArea [ttk::notebook ${viewarea}.frame.tabArea]

set description [frame ${tabArea}.description]
set description.title [label ${description}.title -textvariable currentPackage(title) -font TkHeadingFont -justify left]
set description.caption [label ${description}.caption -textvariable currentPackage(caption) -justify left]
set description.longText [label ${description}.longText -textvariable currentPackage(longText) -justify left -anchor w]

bind . <Configure> [concat [list ${description.longText} configure -wraplength ] {[expr {[winfo width .]-20}]}]

grid ${description.title} -sticky nw
grid ${description.caption} -sticky nw
grid ${description.longText} -sticky nwes

set reviews [frame ${tabArea}.review]
set feedback [frame ${tabArea}.feedback]

$tabArea add $description -text "Package Description" -sticky news
$tabArea add $reviews -text "Reviews" -state disabled -sticky news
$tabArea add $feedback -text "Feedback" -state disabled -sticky news

pack $tabArea -fill both -expand 1 -side top

# Initialize Filter
set searchQuery ""
set filterCategory ""

set pkgs [getPackList "" ""]

drawPackageList $canvas $pkgs