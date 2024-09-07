#/usr/bin/env bash

#install dcaconv-completion.bash ~/.local/share/bash-completion/completions/dcaconv-completion.bash
#source dcaconv-completion.bash

_dcaconv_completions()
{
	local cur prev words cword
	_init_completion || return
	
	case $prev in
		--help|--version|--long|--loop|--trim-loop-end|--verbose|--rate|--channels|--loop-start|--loop-end|--stereo|\
		-!(-*)[hvLlEVrcseS])
			return
			;;
		-i|--in)
			_filedir "@(wav|ogg|flac|mp3|dca)"
			return
			;;
		-o|--out)
			_filedir "@(dca|wav)"
			return
			;;
		-p|--preview)
			_filedir "@(wav)"
			return
			;;
		-f|--format)
			COMPREPLY=($(compgen -W "adpcm pcm8 pcm16" "$cur"))
			return
			;;
		-t|--trim)
			COMPREPLY=($(compgen -W "both start end" "$cur"))
			return
			;;
		*)
			
			#This is the suggestion if not suggesting for one of the above. It suggests supported options.
			COMPREPLY=($(compgen -W "--in --out --preview --format --rate --channels --stereo --loop --loop-start --loop-end --trim --long --trim-loop-end --verbose --version" -- "$cur"))
			return
			;;
		
	esac
} && complete -F _dcaconv_completions dcaconv
