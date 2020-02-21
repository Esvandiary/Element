using System;
using Element;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using CommandLine;
using Newtonsoft.Json;

namespace Alchemist
{
	/// <summary>
	/// Base class for writing Alchemist commands.
	/// </summary>
	internal abstract class BaseCommand
	{
		[Option("no-prelude", Default = false, HelpText = "Prelude functionality is included. Warning: Without Prelude only unverified compiler intrinsics will be available.")]
		public bool ExcludePrelude { get; set; }

		[Option("packages", Required = false, HelpText = "Element packages to load into the context.")]
		public IEnumerable<string> Packages { get; set; }

		[Option("source-files", Required = false, HelpText = "Extra individual source files to load into the context.")]
		public IEnumerable<string> ExtraSourceFiles { get; set; }
		
		[Option("debug", Required = false, Default = false, HelpText = "Preserves debug information while compiling.")]
		public bool Debug { get; set; }
		
		[Option("verbosity", Required = false, Default = MessageLevel.Information, HelpText = "Verbosity of compiler messages.")]
		public MessageLevel Verbosity { get; set; }

		[Option("logjson", Required = false, Default = false, HelpText = "Serializes log messages structured as Json instead of plain string.")]
		public bool LogMessagesAsJson { get; set; }
		
		protected abstract bool _skipValidation { get; }

		private DirectoryInfo GetPackageDirectories(string package)
		{
			var directoryInfo = Directory.Exists(package) ? new DirectoryInfo(package) : null;
			if (directoryInfo != null) return directoryInfo;
			Log($"Package directory \"{package}\" doesn't exist.", MessageLevel.Error);
			return null;
		}

		public int Invoke(string args)
		{
			Log(args);
			var (exitCode, result) = CommandImplementation(new CompilationInput(Log)
			{
				Debug = Debug,
				SkipValidation = _skipValidation,
				Verbosity = Verbosity,
				ExcludePrelude = ExcludePrelude,
				Packages = Packages.Select(GetPackageDirectories).ToList(),
				ExtraSourceFiles = ExtraSourceFiles.Select(file => new FileInfo(file)).ToList()
			});
			Log(result);
			return exitCode;
		}

		private void Log(string message, MessageLevel? level = default) => Log(new CompilerMessage(message, level));

		private void Log(CompilerMessage message)
		{
			var msg = LogMessagesAsJson ? JsonConvert.SerializeObject(message) : message.ToString();
			if (message.MessageLevel >= MessageLevel.Error) Console.Error.WriteLine(msg);
			else Console.WriteLine(msg);
		}

		protected abstract (int ExitCode, string Result) CommandImplementation(in CompilationInput input);
	}
}