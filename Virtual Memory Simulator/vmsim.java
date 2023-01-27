
/**
 * @File: vmsim.java
 * @Author: Quan Nguyen
 * @Project: Virtual Memory Simulator
 * @Class: CSC452
 * @Purpose: This program will implement various page replacement algorithms
 *           that an Operating System implementer may choose to use and compare
 *           the results of four different algorithms on traces of memory
 *           references. It will write up your results and provide a graph that
 *           compares the performance of the various algorithms. The four
 *           algorithms for this project are: 
 *           - Opt - Simulate what the optimal page replacement algorithm would 
 *           choose if it had perfect knowledge
 *           - Clock - Use the better implementation of the second-chance
 *           algorithm 
 *           - FIFO - Evict the oldest page in memory 
 *           - Random - Pick a page at random. 
 *           The implementation will be a single-level page table for a 32-bit address space.
 *           All pages will be 2KB in size. The number of frames will be a parameter to the
 *           execution of the program. 
 *           The program will run with this command: 
 *           java vmsim -n <numframes> -a <opt|clock|fifo|rand> <tracefile>
 */
import java.io.File;
import java.io.FileNotFoundException;
import java.util.*;

public class vmsim {
	/**
	 * A wrapper class for each table page entry for the Page Table Entry.
	 * 
	 * @author Quan Nguyen
	 *
	 */
	static class PageTableEntry {

		boolean dirty;
		boolean valid;
		boolean ref;
		int frame;

		public PageTableEntry() {
			this.dirty = false;
			this.valid = false;
			this.ref = true;
			this.frame = -1;
		}

		public void evict() {
			this.dirty = false;
			this.valid = false;
			this.ref = true;
			this.frame = -1;
		}
	}

	private static List<Character> basicDataCommand = Arrays.asList(new Character[] { 'I', 'M', 'S', 'L' });
	// page size is 2K per page
	private static int PAGE_SIZE = 2048;
	private static int TABLE_SIZE = (int) Math.pow(2, 32 - 11);
	// an entry in real world application should have 4 bytes, 3 bits for dirty,
	// valid, ref, 22 bits for frame, and the rest
	// for protection. However, this is a sim so the actual size of entry here might
	// be larger
	private static int PAGE_ENTRY_SIZE = 4;
	private static int memoryAccess = 0;
	private static int pageFault = 0;
	private static int writeToDisk = 0;

	public static void main(String[] args) {
		PageTableEntry[] pageTable = new PageTableEntry[TABLE_SIZE];
		for (int i = 0; i < TABLE_SIZE; i++) {
			pageTable[i] = new PageTableEntry();
		}
		// check basic arguments
		if (args.length != 5 || !args[0].equals("-n") || !args[2].equals("-a")) {
			invalidArgument(args);
		}
		// check if the numframes is true
		int numFrames = 0;
		try {
			numFrames = Integer.parseInt(args[1]);
		} catch (NumberFormatException nfe) {
			invalidArgument(args);
		}

		// check to see if the file is correct
		String algorithm = args[3];
		Scanner getInput = null;
		try {
			getInput = new Scanner(new File(args[4]));
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
		// check to see if the algorithm is good or not, execute the algorithm
		// since we go for speed here, there are a lot of code duplication but
		// that is acceptable to me since there are no unnecessary if/else clause
		// to determine the algorithm
		int[] memory = new int[numFrames];
		switch (algorithm) {
		case "opt":
			opt(getInput, pageTable, memory, numFrames);
			break;
		case "clock":
			clock(getInput, pageTable, memory, numFrames);
			break;
		case "rand":
			rand(getInput, pageTable, memory, numFrames);
			break;
		case "fifo":
			Queue<Integer> ram = new LinkedList<Integer>();
			fifo(getInput, pageTable, ram, numFrames);
			break;
		default:
			invalidArgument(args);
		}
		// print out the result
		System.out.printf(
				"Algorithm: %s\n" + "Number of frames: %d\n" + "Total memory accesses: %d\n" + "Total page faults: %d\n"
						+ "Total writes to disk: %d\n" + "Total size of page table: %d bytes\n",
				algorithm, numFrames, memoryAccess, pageFault, writeToDisk, TABLE_SIZE * PAGE_ENTRY_SIZE);
	}

	/**
	 * Processing the operation effect to the memory and the page
	 * 
	 * @param entry
	 * @param operation
	 */
	private static void processOperation(PageTableEntry entry, String operation) {
		// process operation's effect to the memory and the page
		if (operation.equals("M")) {
			entry.dirty = true;
			memoryAccess += 2;
		} else {
			memoryAccess++;
			if (operation.equals("S")) {
				entry.dirty = true;
			}
		}
	}

	/**
	 * Implementing the fifo algorithm for page replacement. It would evict the
	 * oldest page in memory. Thus, in this case, the RAM or the memory behave
	 * exactly like a queue. A queue will represent for easier implementation =
	 * 
	 * @param getInput  the Scanner to read the content of the trace file
	 * @param pageTable a single-level page table which hold the entry representing
	 *                  the virtual memory
	 * @param ram       the memory, in this case, it would behave exactly like a
	 *                  queue
	 * @param numFrames
	 */
	private static void fifo(Scanner getInput, PageTableEntry[] pageTable, Queue<Integer> ram, int numFrames) {
		int pageSoFar = 0;
		while (getInput.hasNextLine()) {
			String line = getInput.nextLine().trim();
			if (line.length() == 0 || !(basicDataCommand.contains(line.charAt(0))) ) {
				continue;
			}
			// processing each of the operation
			String[] command = line.split("\\s+|,");
			String operation = command[0];
			// get the address space, this is a long
			long address = Long.parseLong(command[1], 16);
			int tableIndex = (int) (address / PAGE_SIZE);
			PageTableEntry entry = pageTable[tableIndex];
			if (entry.valid) {
				System.out.println(line + " (hit)");
			} else {
				if (pageSoFar >= numFrames) { // this should have some duplication in the queue already
					// this should only be reached if queue is full, and there are page to evict
					int indexEvict = ram.poll();
					PageTableEntry entryEvict = pageTable[indexEvict];
					entry.frame = entryEvict.frame;
					entry.valid = true;
					if (entryEvict.dirty) {
						writeToDisk++;
						System.out.println(line + " (page fault - evict dirty)");
					} else {
						System.out.println(line + " (page fault - evict clean)");
					}
					// after done, we just evicting the current Page in ram
					entryEvict.evict();
					// add to ram the current Page
					ram.add(tableIndex);
					pageFault++;
				} else { // first in queue, it most likely will be the number of frame
					System.out.println(line + " (page fault - no eviction)");
					entry.valid = true;
					entry.frame = pageSoFar;
					ram.add(tableIndex); // indicate that the ram is holding this page
					pageSoFar++;
					pageFault++;
				}
			}
			processOperation(entry, operation);
		}
		getInput.close();
	}

	/**
	 * Implementing the optimal algorithm for the page replacement. This will
	 * replace the page in the RAM which have the farthest date in the future to be
	 * used again. A page will get a second chance when it is a hit. There will be a
	 * map to keep track the occurence of the page in throughout database and
	 * tracing.
	 * 
	 * @param getInput  the Scanner to read the content of the trace file
	 * @param pageTable a single-level page table which hold the entry representing
	 *                  the virtual memory
	 * @param ram       the memory, in this case, it would behave exactly like a
	 *                  queue
	 * @param numFrames
	 */
	private static void opt(Scanner getInput, PageTableEntry[] pageTable, int[] ram, int numFrames) {
		int pageSoFar = 0;
		int oldestIndex = 0;
		HashMap<Integer, TreeSet<Integer>> pageMap = new HashMap<Integer, TreeSet<Integer>>();
		ArrayList<Object[]> pageList = new ArrayList<Object[]>();
		// creating and initialize the pageMap, which inlude the occurence of the page
		// in the future,
		// also a list of each line of the file in order to keeping tracks.
		int commandNum = 1;
		while (getInput.hasNextLine()) {
			String line = getInput.nextLine().trim();
			if (line.length() == 0 || !(basicDataCommand.contains(line.charAt(0)))) {
				continue;
			}
			// processing each of the operation
			String[] command = line.split("\\s+|,");
			String operation = command[0];
			// get the address space, this is a long
			long address = Long.parseLong(command[1], 16);
			int tableIndex = (int) (address / PAGE_SIZE);
			if (pageMap.get(tableIndex) == null) {
				pageMap.put(tableIndex, new TreeSet<Integer>());
			}
			pageMap.get(tableIndex).add(commandNum);
			pageList.add(new Object[] { tableIndex, operation, line });
			commandNum++;
		}

		getInput.close();
		commandNum = 1;
		for (Object[] step : pageList) {
			int tableIndex = (int) step[0];
			String operation = (String) step[1];
			String line = (String) step[2];
			PageTableEntry entry = pageTable[tableIndex];
			if (entry.valid) {
				System.out.println(line + " (hit)");
				entry.ref = true; // we give a second chance a hit happen hit
			} else {
				if (pageSoFar >= numFrames) { // this should have some duplication in the ram already
					// this should only be reached if ram is full, and there are page to evict
					int pageFarthest = -1;
					int maximum = -1;
					// Finding the farthest page in the future which will be used again
					for (int page : ram) {
						int nextOcc;
						if (pageMap.get(page).isEmpty()) {
							nextOcc = -1;
							break;
						} else {
							nextOcc = pageMap.get(page).first();
							while (nextOcc <= commandNum) {

								pageMap.get(page).remove(nextOcc);
								if (pageMap.get(page).isEmpty()) {
									nextOcc = -1;
									break;
								} else {
									nextOcc = pageMap.get(page).first();
								}
							}
						}
						if (nextOcc == -1) { // this happen if the page will be never used again in the future
							// just use this page instead
							pageFarthest = page;
							break;
						}
						if (nextOcc > maximum) {
							pageFarthest = page;
							maximum = nextOcc;
						}
					}
					PageTableEntry entryEvict;
					if (pageFarthest == -1) {
						entryEvict = pageTable[ram[0]];
					} else {
						entryEvict = pageTable[pageFarthest];
					}
					// after this, we finally found the entry to evict
					entry.frame = entryEvict.frame;
					entry.valid = true;
					if (entryEvict.dirty) {
						writeToDisk++;
						System.out.println(line + " (page fault - evict dirty)");
					} else {
						System.out.println(line + " (page fault - evict clean)");
					}
					entryEvict.evict();
					// replace the current hand with the new one, increment current
					ram[entry.frame] = tableIndex;
					pageFault++;
				} else { // first in queue, it most likely will be the number of frame
					System.out.println(line + " (page fault - no eviction)");
					entry.valid = true;
					entry.frame = pageSoFar;
					ram[pageSoFar] = tableIndex;
					pageSoFar++;
					pageFault++;
				}

			}
			processOperation(entry, operation);
			commandNum++;
		}

	}

	/**
	 * Implementing the clock algorithm for page replacement. Use the implementation
	 * of the second-chance algorithm. A page will get a second chance when it is a
	 * hit.
	 * 
	 * @param getInput  the Scanner to read the content of the trace file
	 * @param pageTable a single-level page table which hold the entry representing
	 *                  the virtual memory
	 * @param ram       the memory, in this case, it would behave exactly like a
	 *                  queue
	 * @param numFrames
	 */
	private static void clock(Scanner getInput, PageTableEntry[] pageTable, int[] ram, int numFrames) {
		int pageSoFar = 0;
		int oldestIndex = 0;
		while (getInput.hasNextLine()) {
			String line = getInput.nextLine().trim();
			if (line.length() == 0 || !(basicDataCommand.contains(line.charAt(0)))) {
				continue;
			}
			// processing each of the operation
			String[] command = line.split("\\s+|,");
			String operation = command[0];
			// get the address space, this is a long
			long address = Long.parseLong(command[1], 16);
			int tableIndex = (int) (address / PAGE_SIZE);
			PageTableEntry entry = pageTable[tableIndex];
			if (entry.valid) {
				System.out.println(line + " (hit)");
				entry.ref = true; // we give a second chance a hit happen hit
			} else {
				if (pageSoFar >= numFrames) { // this should have some duplication in the ram already
					// this should only be reached if ram is full, and there are page to evict
					int indexEvict = ram[oldestIndex];
					PageTableEntry entryEvict = pageTable[indexEvict];
					// go through, increment hand until there are page with no ref
					while (entryEvict.ref) {
						entryEvict.ref = false;
						oldestIndex++;
						if (oldestIndex >= ram.length) {
							oldestIndex = 0;
						}
						indexEvict = ram[oldestIndex];
						entryEvict = pageTable[indexEvict];
					}
					// after this, we finally found the entry to evict
					entry.frame = entryEvict.frame;
					entry.valid = true;
					if (entryEvict.dirty) {
						writeToDisk++;
						System.out.println(line + " (page fault - evict dirty)");
					} else {
						System.out.println(line + " (page fault - evict clean)");
					}
					entryEvict.evict();
					// replace the current hand with the new one, increment current
					ram[oldestIndex] = tableIndex;
					oldestIndex++;
					if (oldestIndex >= ram.length) {
						oldestIndex = 0;
					}
					pageFault++;
				} else { // first in queue, it most likely will be the number of frame
					System.out.println(line + " (page fault - no eviction)");
					entry.valid = true;
					entry.frame = pageSoFar;
					ram[pageSoFar] = tableIndex;
					pageSoFar++;
					pageFault++;
				}
			}

			processOperation(entry, operation);
		}
		getInput.close();
	}

	/**
	 * Implementing the random algorithm for page replacement. It would evict a
	 * random page in the memory
	 * 
	 * @param getInput  the Scanner to read the content of the trace file
	 * @param pageTable a single-level page table which hold the entry representing
	 *                  the virtual memory
	 * @param ram       the memory, in this case, it would behave exactly like a
	 *                  queue
	 * @param numFrames
	 */
	private static void rand(Scanner getInput, PageTableEntry[] pageTable, int[] ram, int numFrames) {
		Random random = new Random();
		int pageSoFar = 0;
		while (getInput.hasNextLine()) {
			String line = getInput.nextLine().trim();
			if ( line.length() == 0 || !(basicDataCommand.contains(line.charAt(0))) ) {
				continue;
			}
			// processing each of the operation
			String[] command = line.split("\\s+|,");
			String operation = command[0];
			// get the address space, this is a long
			long address = Long.parseLong(command[1], 16);
			int tableIndex = (int) (address / PAGE_SIZE);
			PageTableEntry entry = pageTable[tableIndex];
			if (entry.valid) {
				System.out.println(line + " (hit)");
			} else {
				if (pageSoFar >= numFrames) { // this should have some duplication in the queue already
					// this should only be reached if queue is full, and there are page to evict
					int memoryToEvict = random.nextInt(ram.length);
					int indexEvict = ram[memoryToEvict];
					PageTableEntry entryEvict = pageTable[indexEvict];
					entry.frame = entryEvict.frame;
					entry.valid = true;
					if (entryEvict.dirty) {
						writeToDisk++;
						System.out.println(line + " (page fault - evict dirty)");
					} else {
						System.out.println(line + " (page fault - evict clean)");
					}
					// after done, we just evicting the current Page in ram
					entryEvict.evict();
					// add to ram the current Page
					ram[memoryToEvict] = tableIndex;
					pageFault++;
				} else { // first in queue, it most likely will be the number of frame
					System.out.println(line + " (page fault - no eviction)");
					entry.valid = true;
					entry.frame = pageSoFar;
					ram[pageSoFar] = tableIndex; // indicate that the ram is holding this page
					pageSoFar++;
					pageFault++;
				}
			}
			processOperation(entry, operation);
		}
		getInput.close();
	}

	/**
	 * Print out the error message if the program has invalid arguments format. Exit
	 * the program
	 */
	private static void invalidArgument(String[] args) {
		System.out.println(Arrays.toString(args));
		System.out.println("vmsim -n <numframes> -a <opt|clock|fifo|rand> <tracefile>");
		System.exit(1);
	}
}
