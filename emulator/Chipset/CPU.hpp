#pragma once
#include "../Config.hpp"

#include "../Logger.hpp"

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>

/**
 * We need to setup break-point system in CPU 
 * 
 */


namespace casioemu
{
	class Emulator;

	class CPU
	{
		Emulator &emulator;

	private:
		struct RegisterStub
		{
			size_t type_size;
			std::string name;

			uint16_t raw;
		};

		struct Instruction
		{
			size_t address;

			std::string code;
			Instruction *next_instruction = nullptr;

			Instruction(size_t address = 0)
					: address(address) {}
		};

		struct BasicBlock
		{
			// Type of the basic block.
			// BB_TYPE_FUNCTION takes priority
			enum
			{
				// This type shouldn't exist
				BB_TYPE_UNKNOWN = 0x0,
				// When a basic block is found via a branch and link (BL) instruction,
				BB_TYPE_FUNCTION = 0x1,
				// When a basic block is found via a branch instruction (B)
				BB_TYPE_BRANCH = 0x2,
			} type;

			// Absolute address (CSR << 16 | PC) of the block.
			size_t address;
			std::string bb_name;

			// A basic block is a block of continuous code that is executed.
			// It ends when a branch instruction is executed.
			// It also means that this is the target of a branch instruction.
			Instruction *first_instruction = nullptr;
			Instruction *last_instruction = nullptr;

			// If this is a function, points to the first basic block
			BasicBlock *first_basic_block = nullptr;
			BasicBlock *last_basic_block = nullptr;
			BasicBlock(size_t address)
					: type(BB_TYPE_BRANCH), address(address) {}
		};

		template<typename value_type>
		struct Register : public RegisterStub
		{
			Register<value_type>()
			{
				type_size = sizeof(value_type);
				name = "?";
			}

			operator value_type()
			{
				return raw;
			}

			Register<value_type> &operator =(value_type value)
			{
				raw = value;
				return *this;
			}

			Register<value_type> &operator &=(value_type value)
			{
				return *this = raw & value;
			}

			Register<value_type> &operator |=(value_type value)
			{
				return *this = raw | value;
			}

			Register<value_type> &operator ^=(value_type value)
			{
				return *this = raw ^ value;
			}

			Register<value_type> &operator +=(value_type value)
			{
				return *this = raw + value;
			}

			Register<value_type> &operator -=(value_type value)
			{
				return *this = raw - value;
			}
		};

		typedef Register<uint8_t> reg8_t;
		typedef Register<uint16_t> reg16_t;

		uint8_t impl_last_dsr;
		uint8_t impl_flags_changed, impl_flags_out, impl_flags_in;
		uint8_t impl_shift_buffer;
		uint16_t impl_opcode, impl_long_imm;

		static std::string uint64_to_hex(uint64_t value)
		{
			std::ostringstream output;
			output << "0x" << std::hex << value;
			return output.str();
		}

		struct Operand {
			uint64_t value;
			size_t register_index, register_size;
			std::string PrintOperand() const
			{
				std::string output;
				switch (register_size)
				{
				case 0:
					output = uint64_to_hex(value);
					break;
				case 1:
					output = "r" + std::to_string(register_index) + " /*" + uint64_to_hex((uint8_t)value) + "*/";
					break;
				case 2:
					output = "er" + std::to_string(register_index) + " /*" + uint64_to_hex((uint16_t)value) + "*/";
					break;
				case 4:
					output = "xr" + std::to_string(register_index) + " /*" + uint64_to_hex(value) + "*/";
					break;
				default:
					output = "unknown register size " + std::to_string(register_size);
					break;
				}
				return output;
			}
		} impl_operands[2];
		size_t impl_hint;
		uint16_t impl_csr_mask;

		void SetupOpcodeDispatch();
		void SetupRegisterProxies();

	public:
		CPU(Emulator &emulator);
		~CPU();
		void SetupInternals();

		/**
		 * See 1.2.2.1 in the nX-U8 manual.
		 */
		enum StatusFlag
		{
			PSW_C = 0x80,
			PSW_Z = 0x40,
			PSW_S = 0x20,
			PSW_OV = 0x10,
			PSW_MIE = 0x8,
			PSW_HC = 0x4,
			PSW_ELEVEL = 0x3
		};

		/**
		 * See 1.3.6 in the nX-U8 manual.
		 */
		enum MemoryModel
		{
			MM_SMALL,
			MM_LARGE
		} memory_model;

		/**
		 * See 1.2.1 in the nX-U8 manual.
		 */
		reg8_t reg_r[16], reg_cr[16];
		reg16_t reg_pc, reg_elr[4], &reg_lr;
		reg16_t reg_csr, reg_ecsr[4], &reg_lcsr;
		reg8_t reg_epsw[4], &reg_psw;
		reg16_t reg_sp, reg_ea;
		reg8_t reg_dsr;

		// CSR << 16 | PC to BasicBlock map.
		std::map<size_t, BasicBlock*> basic_blocks;
		std::map<size_t, Instruction*> instructions;
		BasicBlock *current_basic_block = nullptr;

		BasicBlock *GetBasicBlock(size_t address)
		{
			auto it = basic_blocks.find(address);
			if (it != basic_blocks.end())
				return it->second;
			return nullptr;
		}

		BasicBlock *CreateBasicBlock(size_t address)
		{
			BasicBlock *bb = GetBasicBlock(address);
			if (bb == nullptr)
			{
				bb = new BasicBlock(address);
				basic_blocks[address] = bb;
			}
			return bb;
		}

		/** Add an instruction to a basic block.
		 * If the basic block is empty, the instruction will be the first and last
		 * instruction in the block.
		 */
		void BasicBlockAddInstruction(BasicBlock *bb, Instruction *instruction)
		{
			if (bb->first_instruction == nullptr)
			{
				bb->first_instruction = instruction;
				bb->last_instruction = instruction;
			}
			else
			{
				bb->last_instruction->next_instruction = instruction;
				bb->last_instruction = instruction;
			}
		}

		void BasicBlockAppendBasicBlock(BasicBlock *bb, BasicBlock *next_bb)
		{
			if (bb->last_basic_block == nullptr)
			{
				bb->first_basic_block = next_bb;
				bb->last_basic_block = next_bb;
			}
			else
			{
				bb->last_basic_block->last_basic_block = next_bb;
				bb->last_basic_block = next_bb;
			}
		}

		Instruction *GetInstruction(size_t address)
		{
			auto it = instructions.find(address);
			if (it != instructions.end())
				return it->second;
			return nullptr;
		}

		Instruction *CreateInstruction(size_t address)
		{
			Instruction *instruction = GetInstruction(address);
			if (instruction == nullptr)
			{
				instruction = new Instruction();
				instruction->address = address;
				instructions[address] = instruction;
			}
			return instruction;
		}

		void MakeBasicBlockFunction(BasicBlock *bb)
		{
			bb->type = BasicBlock::BB_TYPE_FUNCTION;
		}

		std::string CreateFunctionName(size_t address)
		{
			std::ostringstream output;
			output << "fun_" << std::hex << std::setfill('0') << std::uppercase
				<< std::setw(6) << address;
			return output.str();
		}

		std::string CreateLabelName(size_t address)
		{
			std::ostringstream output;
			output << "label_" << std::hex << std::setfill('0') << std::uppercase
				<< std::setw(6) << address;
			return output.str();
		}

		std::string PrintAllBasicBlocks() const
		{
			std::ostringstream output;
			for (const auto &pair : basic_blocks)
			{
				const BasicBlock *bb = pair.second;
				output << "Basic Block at " << std::hex << std::setfill('0') << std::uppercase
					<< std::setw(6) << bb->address << ": " << bb->bb_name << "\n";
				if (bb->first_instruction)
				{
					output << "  Instructions:\n";
					for (Instruction *ins = bb->first_instruction; ins; ins = ins->next_instruction)
					{
						output << "    " << std::hex << std::setfill('0') << std::uppercase
							<< std::setw(6) << ins->address << ": " << ins->code;
					}
				}
			}
			return output.str();
		}

		void SetMemoryModel(MemoryModel memory_model);
		void Next();
		void Reset();
		void Raise(size_t exception_level, size_t index);
		size_t GetExceptionLevel();
		bool GetMasterInterruptEnable();
		std::string GetBacktrace() const;

	private:
		struct StackFrame
		{
			bool lr_pushed;
			uint16_t lr_push_address, new_csr, new_pc;
		};
		std::vector<StackFrame> stack;

		

		uint16_t Fetch();

		enum OpcodeHint
		{
			H_IE = 0x0001, // * Extend Immediate flag for arithmetic instructions.
			H_ST = 0x0002, // * Store flag for load/store/coprocessor instructions.
			H_DW = 0x0004, // * Store a new DSR value.
			H_DS = 0x0008, // * Instruction is a DSR prefix.
			H_IA = 0x0010, // * Increment EA flag for load/store/coprocessor instructions.
			H_TI = 0x0020, // * Instruction takes an external long immediate value.
			H_WB = 0x0040  // * Register Writeback flag for a lot of instructions to make life easier.
		};

		struct OpcodeSource
		{
			void (CPU::*handler_function)();
			/**
			 * I know this should be an OpcodeHint, but the damn C++ initializer lists
			 * convert literally everything to int if it's more than a single enum
			 * value. Even binary OR'd values and 0. Pain in the ass.
			 */
			size_t hint;
			uint16_t opcode;
			struct OperandMask
			{
				/**
				 * `register_size` determines whether an operand is a register
				 * or an immediate. If it's 0, the operand is an immediate. Otherwise
				 * the operand is a register of size `register_size`.
				 */
				size_t register_size;
				uint16_t mask, shift;
			} operands[2];
		};
		static OpcodeSource opcode_sources[];
		OpcodeSource **opcode_dispatch;

		typedef RegisterStub CPU::*RegisterStubPointer;
		typedef RegisterStub (CPU::*RegisterStubArrayPointer)[];
		struct RegisterRecord
		{
			std::string name;
			size_t array_size, array_base;
			RegisterStubPointer stub;
			RegisterStubArrayPointer stub_array;
		};
		static RegisterRecord register_record_sources[];
		std::map<std::string, RegisterStub *> register_proxies;

		// * Arithmetic Instructions
		void OP_ADD();
		void OP_ADD16();
		void OP_ADDC();
		void OP_AND();
		void OP_MOV16();
		void OP_MOV();
		void OP_OR();
		void OP_XOR();
		void OP_CMP16();
		void OP_SUB();
		void OP_SUBC();
		void Add8();
		void ZSCheck();
		void ShiftLeft8();
		void ShiftRight8();
		// * Shift Instructions
		void OP_SLL();
		void OP_SLLC();
		void OP_SRA();
		void OP_SRL();
		void OP_SRLC();
		// * Load/Store Instructions
		void OP_LS_EA();
		void OP_LS_R();
		void OP_LS_I_R();
		void OP_LS_BP();
		void OP_LS_FP();
		void OP_LS_I();
		void LoadStore(uint16_t offset, size_t length);
		// * Control Register Access Instructions
		void OP_ADDSP();
		void OP_CTRL();
		// * PUSH/POP Instructions
		void OP_PUSH();
		void OP_PUSHL();
		void OP_POP();
		void OP_POPL();
		void Push16(uint16_t data);
		uint16_t Pop16();
		// * Coprocessor Data Transfer Instructions
		void OP_CR_R();
		void OP_CR_EA();
		void BumpEA(size_t value_size);
		// * EA Register Data Transfer Instructions
		void OP_LEA();
		// * ALU Instructions
		void OP_DAA();
		void OP_DAS();
		void OP_NEG();
		// * Bit Access Instructions
		void OP_BITMOD();
		// * PSW Access Instructions
		void OP_PSW_OR();
		void OP_PSW_AND();
		void OP_CPLC();
		// * Conditional Relative Branch Instructions
		void OP_BC();
		// * Sign Extension Instruction
		void OP_EXTBW();
		// * Software Interrupt Instructions
		void OP_SWI();
		void OP_BRK();
		// * Branch Instructions
		void OP_B();
		void OP_BL();
		// * Multiplication and Division Instructions
		void OP_MUL();
		void OP_DIV();
		// * Miscellaneous Instructions
		void OP_INC_EA();
		void OP_DEC_EA();
		void OP_RT();
		void OP_RTI();
		void OP_NOP();
		void OP_DSR();
	};
}

#include <iostream>
extern "C"
{
	static void PrintAllBasicBlocks(casioemu::CPU *cpu)
	{
		if (cpu)
		{
			std::string output = cpu->PrintAllBasicBlocks();
			std::cout << output;
		}
	}
}