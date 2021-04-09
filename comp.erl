%Práctica 3 Concurriencia y paralelismo
%Autor: Javier París Rojo
%Fecha: 28/03/2020

-module(comp).

-export([comp/1, comp/2 , decomp/1, decomp/2, comp_proc/2, comp_proc/3,
		comp_loop/3, decomp_proc/2, decomp_proc/3, decomp_loop/3]).


-define(DEFAULT_CHUNK_SIZE, 1024*1024).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
					% Código original %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 

%%% File Compression

comp(File) -> %% Compress file to file.ch
    comp(File, ?DEFAULT_CHUNK_SIZE).

comp(File, Chunk_Size) ->  %% Starts a reader and a writer which run in separate processes
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    comp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

comp_loop(Reader, Writer) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop(Reader, Writer);
        eof ->  %% end of file, stop reader and writer
            Reader ! stop,
            Writer ! stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n",[Reason]),
            Reader ! stop,
            Writer ! abort
    end.

%% File Decompression

decomp(Archive) ->
    decomp(Archive, string:replace(Archive, ".ch", "", trailing)).

decomp(Archive, Output_File) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    decomp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

decomp_loop(Reader, Writer) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop(Reader, Writer);
        eof ->    %% end of file => exit decompression
            Reader ! stop,
            Writer ! stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.
    
    

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%   
				% Compresion utilizando n procesos. %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%   

comp_proc(File, Procs) -> %% Compress file to file.ch
    comp_proc(File, ?DEFAULT_CHUNK_SIZE, Procs).

comp_proc(File, Chunk_Size, Procs) ->  %% Starts a reader and a writer which run in separate processes
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    crear_procs(Writer, Reader, Procs), %Creamos los procesos
                    stop(Writer, Reader, Procs);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

%Función que crea los procesos.
crear_procs(Writer, Reader, Procs)->
    if  Procs == 0 -> ok;
        
        true -> spawn_link(?MODULE, comp_loop, [Reader, Writer, self()]),
            crear_procs(Writer, Reader, (Procs - 1))
    end.


comp_loop(Reader, Writer, From) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop(Reader, Writer, From);
        eof ->  %% end of file, stop reader and writer
            From ! ok_stop;
        {error, Reason} ->
            From ! {error, Reason}
    end.
    


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%   
				% Descompresión utilizando n procesos. %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%   


decomp_proc(Archive, Procs) ->
    decomp_proc(Archive, string:replace(Archive, ".ch", "", trailing),Procs).


decomp_proc(Archive, Output_File, Procs) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    crear_procs(Writer, Reader, Procs, self()), %Creamos los procesos
                    stop(Writer, Reader, Procs);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.
    

%Función que crea los procesos.
crear_procs(Writer, Reader, Procs, From) ->
    if  Procs == 0 -> ok;
        
        true -> spawn(?MODULE, decomp_loop, [Reader, Writer, From]),
            crear_procs(Writer, Reader, (Procs - 1), From)
    end.
    

decomp_loop(Reader, Writer, From) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop(Reader, Writer,From);
        eof ->    %% end of file => exit decompression
            From ! ok_stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            From ! {error, Reason}
    end.
    
    
    
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%   
				% Funciones auxiliares compartidas. %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
    
stop(Writer, Reader, Procs) ->
    receive
        ok_stop ->
            if Procs == 1 -> 
                Reader ! stop,
                Writer ! stop;
            true-> stop(Writer, Reader, Procs - 1)
            end;

        {error, Reason} ->
            if Procs == 1 ->
                io:format("Error reading input file: ~w~n",[Reason]), 
                Reader ! stop,
                Writer ! abort;
            true -> stop(Writer, Reader, Procs - 1)
            end
            
    end.
