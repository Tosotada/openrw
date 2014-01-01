#define GLEW_STATIC
#include <GL/glew.h>

#include <engine/GameWorld.hpp>
#include <loaders/LoaderDFF.hpp>
#include <render/DebugDraw.hpp>
#include <render/Model.hpp>
#include <ai/GTAAIController.hpp>
#include <ai/GTAPlayerAIController.hpp>
#include <objects/GTACharacter.hpp>
#include <objects/GTAVehicle.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "MenuSystem.hpp"
#include "State.hpp"
#include <SFML/Graphics.hpp>

#include <memory>
#include <sstream>
#include <iomanip>
#include <getopt.h>
#include <boost/concept_check.hpp>

constexpr int WIDTH  = 800,
              HEIGHT = 600;

constexpr double PiOver180 = 3.1415926535897932384626433832795028/180;

sf::RenderWindow window;

GameWorld* gta = nullptr;

GTAPlayerAIController* player = nullptr;
GTACharacter* playerCharacter = nullptr;

DebugDraw* debugDrawer = nullptr;
GTAObject* debugObject = nullptr;

glm::vec3 plyPos(87.f, -932.f, 58.f);
glm::vec2 plyLook;
glm::vec3 movement;
float moveSpeed = 20.0f;
bool inFocus = false;
bool mouseGrabbed = true;
int debugMode = 0;

sf::Font font;

bool showControls = false;

bool hitWorldRay(glm::vec3& hit, glm::vec3& normal, GTAObject** object = nullptr)
{
	glm::mat4 view;
	view = glm::rotate(view, -90.f, glm::vec3(1, 0, 0));
	view = glm::rotate(view, plyLook.y, glm::vec3(1, 0, 0));
	view = glm::rotate(view, plyLook.x, glm::vec3(0, 0, 1));
	glm::vec3 dir = glm::inverse(glm::mat3(view)) * glm::vec3(0.f, 0.f, 1.f) * -50.f;
	auto from = btVector3(plyPos.x, plyPos.y, plyPos.z);
	auto to = btVector3(plyPos.x+dir.x, plyPos.y+dir.y, plyPos.z+dir.z);
	btCollisionWorld::ClosestRayResultCallback ray(from, to);
	gta->dynamicsWorld->rayTest(from, to, ray);
	if( ray.hasHit() ) 
	{
		hit = glm::vec3(ray.m_hitPointWorld.x(), ray.m_hitPointWorld.y(),
						ray.m_hitPointWorld.z());
		normal = glm::vec3(ray.m_hitNormalWorld.x(), ray.m_hitNormalWorld.y(),
						   ray.m_hitNormalWorld.z());
		if(object) {
			*object = static_cast<GTAObject*>(ray.m_collisionObject->getUserPointer());
		}
		return true;
	}
	return false;
}

void lockCursor(bool lock)
{
	mouseGrabbed = lock;
	window.setMouseCursorVisible(! lock);
}

// Commands.
std::map<std::string, std::function<void (std::string)>> Commands = {
	{"pedestrian-vehicle", 
		[&](std::string) {
			glm::vec3 hit, normal;
			if(hitWorldRay(hit, normal)) {
				auto ped = gta->createPedestrian(2, plyPos+glm::vec3(0.f,10.f,0.f));
				// Pick random vehicle.
				auto it = gta->vehicleTypes.begin();
				std::uniform_int_distribution<int> uniform(0, 9);
				for(size_t i = 0, n = uniform(gta->randomEngine); i != n; i++) {
					it++;
				}
				auto spawnpos = hit + normal;
				auto vehicle = gta->createVehicle(it->first, spawnpos, glm::quat(glm::vec3(0.f, 0.f, -plyLook.x * PiOver180)));
				ped->enterVehicle(vehicle, 0);
			}
		}
	},
	{"player-vehicle",
		[&](std::string) {
			glm::vec3 hit, normal;
			if(hitWorldRay(hit, normal)) {
				if(! playerCharacter) {
					playerCharacter = gta->createPedestrian(1, plyPos+glm::vec3(0.f,10.f,0.f));
					player = new GTAPlayerAIController(playerCharacter);
				}

				// Pick random vehicle.
				auto it = gta->vehicleTypes.begin();
				std::uniform_int_distribution<int> uniform(0, 9);
				for(size_t i = 0, n = uniform(gta->randomEngine); i != n; i++) {
					it++;
				}
				
				auto spawnpos = hit + normal;
				auto vehicle = gta->createVehicle(it->first, spawnpos, glm::quat(glm::vec3(0.f, 0.f, -plyLook.x * PiOver180)));
				playerCharacter->enterVehicle(vehicle, 0);
			}
		}
	},
	{"empty-vehicle",
		[&](std::string) {
			glm::vec3 hit, normal;
			if(hitWorldRay(hit, normal)) {
				// Pick random vehicle.
				auto it = gta->vehicleTypes.begin();
				std::uniform_int_distribution<int> uniform(0, 9);
				for(size_t i = 0, n = uniform(gta->randomEngine); i != n; i++) {
					it++;
				}
				
				auto spawnpos = hit + normal;
				auto vehicle = gta->createVehicle(it->first, spawnpos, glm::quat(glm::vec3(0.f, 0.f, -plyLook.x * PiOver180)));
			}
		}
	},
	{"player",
		[&](std::string) {
			playerCharacter = gta->createPedestrian(1, plyPos);
			player = new GTAPlayerAIController(playerCharacter);
		}
	},
	{"knock-down",
		[&](std::string) {
			for(auto it = gta->pedestrians.begin(); it != gta->pedestrians.end(); ++it) {
				(*it)->changeAction(GTACharacter::KnockedDown);
			}
		}
	},
	{"vehicle-test", 
		[&](std::string) {
			glm::vec3 hit, normal;
			if(hitWorldRay(hit, normal)) {
				glm::vec3 spawnPos = hit + glm::vec3(-5, 0.f, 0.0) + normal;
				size_t k = 1;
				for(std::map<uint16_t, std::shared_ptr<CarData>>::iterator it = gta->vehicleTypes.begin();
					it != gta->vehicleTypes.end(); ++it) {
					if(it->first == 140) continue; // get this plane out of here.
					gta->createVehicle(it->first, spawnPos);
					spawnPos += glm::vec3(5, 0, 0);
					if((k++ % 4) == 0) { spawnPos += glm::vec3(-20, -15, 0); }
				}
			}
		}
	},
	{"pedestrian-test",
		[&](std::string) {
			glm::vec3 hit, normal;
			if(hitWorldRay(hit, normal)) {
				glm::vec3 spawnPos = hit + glm::vec3(-5, 0.f, 0.0) + normal;
				size_t k = 1;
				// Spawn every pedestrian.
				for(auto it = gta->pedestrianTypes.begin();
					it != gta->pedestrianTypes.end(); ++it) {
					gta->createPedestrian(it->first, spawnPos);
					spawnPos += glm::vec3(2.5, 0, 0);
					if((k++ % 6) == 0) { spawnPos += glm::vec3(-15, -2.5, 0); }
				}
			}
		}
	},
	{"list-ipl",
		[&](std::string) {
			for(std::map<std::string, std::string>::iterator it = gta->gameData.iplLocations.begin();
				it != gta->gameData.iplLocations.end();
				++it) {
				gta->logInfo(it->second);
			}
		}
	},
	{"load-ipl",
		[&](std::string line) {
			if(line.find(' ') != line.npos) {
				std::string ipl = line.substr(line.find(' ')+1);
				auto iplit = gta->gameData.iplLocations.find(ipl);
				if(iplit != gta->gameData.iplLocations.end()) {
					gta->logInfo("Loading: " + iplit->second);
					gta->loadZone(iplit->second);
					gta->placeItems(iplit->second);
				}
				else {
					gta->logInfo("Not found: " + ipl);
				}
			}
		}
	},
	{"create-instance",
		[&](std::string line) {
			if(line.find(' ') != line.npos) {
				std::string ID = line.substr(line.find(' ')+1);
				int intID = atoi(ID.c_str());
				auto archit = gta->objectTypes.find(intID);
				if(archit != gta->objectTypes.end()) {
					gta->createInstance(archit->first, plyPos);
				}
				else {
					gta->logInfo("Unkown Object: " + ID);
				}
			}
		}
	},
	{"object-info",
		[&](std::string) {
			glm::vec3 hit, normal;
			GTAObject* object;
			if(hitWorldRay(hit, normal, &object)) {
				debugObject = object;
			}
		}
	},
	{"damage-object",
		[&](std::string) {
			if(debugObject) {
				GTAObject::DamageInfo dmg;
				dmg.type = GTAObject::DamageInfo::Bullet;
				dmg.hitpoints = 15.f;
				debugObject->takeDamage(dmg);
			}
		}
	}
	/*{"",
		[&](std::string) {
			
		}
	}*/
};



void command(const std::string& line)
{
	std::string cmd;
	if(line.find(' ') != line.npos) {
		cmd = line.substr(0, line.find(' '));
	}
	else {
		cmd = line;
	}
	
	auto it = Commands.find(cmd);
	if(it != Commands.end()) {
		it->second(line);
	}
	else {
		gta->logInfo("Unkown command: " + cmd);
	}
}

void handleGlobalEvent(sf::Event &event)
{
	switch (event.type) {
	case sf::Event::KeyPressed:
		break;
	case sf::Event::GainedFocus:
		inFocus = true;
		break;
	case sf::Event::LostFocus:
		inFocus = false;
		break;
	default: break;
	}
}

void handleInputEvent(sf::Event &event)
{
	switch(event.type) {
	case sf::Event::KeyPressed:
		switch (event.key.code) {
		case sf::Keyboard::LShift:
			moveSpeed = 60.f;
			break;
		case sf::Keyboard::Space:
			if(playerCharacter) {
				playerCharacter->jump();
			}
			break;
		case sf::Keyboard::M:
			lockCursor(! mouseGrabbed);
			break;
		case sf::Keyboard::P:
            debugMode+=1;
            while(debugMode > 2) debugMode -= 3;
			break;
		case sf::Keyboard::W:
			movement.y = -1;
			break;
		case sf::Keyboard::S:
			movement.y = 1;
			break;
		case sf::Keyboard::A:
			movement.x = -1;
			break;
		case sf::Keyboard::D:
			movement.x = 1;
			break;
		default: break;
		}
		break;
	case sf::Event::KeyReleased:
		switch(event.key.code) {
		case sf::Keyboard::LShift:
				moveSpeed = 20.f;
				break;
		case sf::Keyboard::W:
			movement.y = 0;
			break;
		case sf::Keyboard::S:
			movement.y = 0;
			break;
		case sf::Keyboard::A:
			movement.x = 0;
			break;
		case sf::Keyboard::D:
			movement.x = 0;
			break;
		case sf::Keyboard::F:
			if(playerCharacter) {
				if(playerCharacter->getCurrentVehicle()) {
					player->exitVehicle();
				}
				else {
					player->enterNearestVehicle();
				}
			}
			break;
		default: break;
		}
		break;
	default: break;
	}
}

void handleCommandEvent(sf::Event &event)
{
	switch(event.type) {
		case sf::Event::KeyPressed:
		switch (event.key.code) {
		case sf::Keyboard::F1:
			showControls = !showControls;
			break;
		case sf::Keyboard::F2:
			command("pedestrian-vehicle");
			break;
		case sf::Keyboard::F3:
			command("player-vehicle");
			break;
		case sf::Keyboard::F4:
			command("empty-vehicle");
			break;
		case sf::Keyboard::F6:
			command("vehicle-test");
			break;
		case sf::Keyboard::F7:
			command("pedestrian-test");
			break;
		case sf::Keyboard::F8:
			command("damage-object");
			break;
		case sf::Keyboard::F9:
			command("object-info");
			break;
		case sf::Keyboard::LBracket:
			gta->gameTime -= 60.f;
			break;
		case sf::Keyboard::RBracket:
			gta->gameTime += 60.f;
			break;
		break;
		default: break;
		}
		default: break;
	}
}

void init(std::string gtapath, bool loadWorld)
{
	// GTA GET
	gta = new GameWorld(gtapath);
	
	// This is harcoded in GTA III for some reason
	gta->gameData.loadIMG("/models/gta3");
	
	gta->load();
	
	// Set time to noon.
	gta->gameTime = 12.f * 60.f;
	
	// Loade all of the IDEs.
	for(std::map<std::string, std::string>::iterator it = gta->gameData.ideLocations.begin();
		it != gta->gameData.ideLocations.end();
		++it) {
		gta->defineItems(it->second);
	}
	
	if(loadWorld) {
		// Load IPLs 
		for(std::map<std::string, std::string>::iterator it = gta->gameData.iplLocations.begin();
			it != gta->gameData.iplLocations.end();
			++it) {
			gta->loadZone(it->second);
			gta->placeItems(it->second);
		}
	}
	
    debugDrawer = new DebugDraw;
    debugDrawer->setShaderProgram(gta->renderer.worldProgram);
    debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe);
    gta->dynamicsWorld->setDebugDrawer(debugDrawer);

	std::cout << "Loaded "
			  << gta->gameData.models.size() << " models, "
			  << gta->gameData.textures.size() << " textures" << std::endl;
}

void update(float dt)
{
	if (inFocus) {
		if (mouseGrabbed) {
			sf::Vector2i screenCenter{sf::Vector2i{window.getSize()} / 2};
			sf::Vector2i mousePos = sf::Mouse::getPosition(window);
			sf::Vector2i deltaMouse = mousePos - screenCenter;
			sf::Mouse::setPosition(screenCenter, window);

			plyLook.x += deltaMouse.x / 10.0;
			plyLook.y += deltaMouse.y / 10.0;

			if (plyLook.y > 90)
				plyLook.y = 90;
			else if (plyLook.y < -90)
				plyLook.y = -90;
		}

		glm::mat4 view;
		view = glm::rotate(view, -90.f, glm::vec3(1, 0, 0));
		view = glm::rotate(view, plyLook.y, glm::vec3(1, 0, 0));
		view = glm::rotate(view, plyLook.x, glm::vec3(0, 0, 1));
		
		if( player != nullptr ) {
			glm::quat playerCamera(glm::vec3(0.f, 0.f, -plyLook.x * PiOver180));
			player->updateCameraDirection(playerCamera);
			player->updateMovementDirection(glm::vec3(movement.x, -movement.y, movement.z));
			player->setRunning(moveSpeed > 21.f);

			float viewDistance = playerCharacter->getCurrentVehicle() ? -3.5f : -2.5f;
			glm::vec3 localView = glm::inverse(glm::mat3(view)) * glm::vec3(0.f, -0.5f, viewDistance);
			if(playerCharacter->getCurrentVehicle()) {
				plyPos = playerCharacter->getCurrentVehicle()->getPosition();
			}
			else {
				plyPos = playerCharacter->getPosition();
			}
			view = glm::translate(view, -plyPos + localView);
		}
		else {
			if (glm::length(movement) > 0.f) {
				plyPos += dt * moveSpeed * (glm::inverse(glm::mat3(view)) * glm::vec3(movement.x, movement.z, movement.y));
			}
			view = glm::translate(view, -plyPos);
		}
		
		gta->gameTime += dt;

		gta->renderer.camera.worldPos = plyPos;
		gta->renderer.camera.frustum.view = view;
		
		// Update all objects.
        for( size_t p = 0; p < gta->pedestrians.size(); ++p) {
			gta->pedestrians[p]->tick(dt);
			
			// For the time being, remove anything that isn't the player with no health.
			if(gta->pedestrians[p]->mHealth <= 0.f) {
				if(gta->pedestrians[p] != playerCharacter) {
					if(gta->pedestrians[p] == debugObject) {
						debugObject = nullptr;
					}
					gta->destroyObject(gta->pedestrians[p]);
					p--;
				}
			}
        }
		for( size_t v = 0; v < gta->vehicleInstances.size(); ++v ) {
			gta->vehicleInstances[v]->tick(dt);
			if(gta->vehicleInstances[v]->mHealth <= 0.f) {
				if(gta->vehicleInstances[v] == debugObject) {
					debugObject = nullptr;
				}
				gta->destroyObject(gta->vehicleInstances[v]);
				v--;
			}
		}
		
		

		gta->dynamicsWorld->stepSimulation(dt, 2, dt);
	}
}

void render()
{
	// Update aspect ratio..
	gta->renderer.camera.frustum.aspectRatio = window.getSize().x / (float) window.getSize().y;
	
	glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	//glEnable(GL_CULL_FACE);

    switch( debugMode ) {
    case 0:
        gta->renderer.renderWorld();
        break;

    case 1: {
		gta->renderer.renderWorld();
        glUseProgram(gta->renderer.worldProgram);
        glm::mat4 proj = gta->renderer.camera.frustum.projection();
        glm::mat4 view = gta->renderer.camera.frustum.view;
        glUniformMatrix4fv(gta->renderer.uniView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(gta->renderer.uniProj, 1, GL_FALSE, glm::value_ptr(proj));
        gta->renderer.renderPaths();
        break;
    }
    case 2: {
        glUseProgram(gta->renderer.worldProgram);
        glm::mat4 proj = gta->renderer.camera.frustum.projection();
        glm::mat4 view = gta->renderer.camera.frustum.view;
        glUniformMatrix4fv(gta->renderer.uniView, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(gta->renderer.uniProj, 1, GL_FALSE, glm::value_ptr(proj));
        gta->dynamicsWorld->debugDrawWorld();
        debugDrawer->drawAllLines();

        break;
    }
    }

	window.resetGLStates();
	
	std::stringstream ss;
	ss << std::setfill('0') << "Time: " << std::setw(2) << gta->getHour() 
		<< ":" << std::setw(2) << gta->getMinute() << std::endl;
	ss << "Game Time: " << gta->gameTime << std::endl;
	ss << "Camera: " << plyPos.x << " " << plyPos.y << " " << plyPos.z << std::endl; 
	
	if(debugObject) {
		auto p = debugObject->getPosition();
		ss << "Position: " << p.x << " " << p.y << " " << p.z << std::endl;
		ss << "Health: " << debugObject->mHealth << std::endl;
		if(debugObject->model) {
			auto m = debugObject->model;
			ss << "Textures: " << std::endl;
			for(auto it = m->geometries.begin(); it != m->geometries.end();
				++it ) 
			{
				auto g = *it;
				for(auto itt = g->materials.begin(); itt != g->materials.end();
					++itt)
				{
					for(auto tit = itt->textures.begin(); tit != itt->textures.end();
						++tit)
					{
						ss << " " << tit->name << std::endl;
					}
				}
			}
		}
		if(debugObject->type() == GTAObject::Vehicle) {
			GTAVehicle* vehicle = static_cast<GTAVehicle*>(debugObject);
			ss << "ID: " << vehicle->info.handling.ID << std::endl;
		}
	}
	
	if(showControls) {
		ss << "F1 - Toggle Help" << std::endl;
		ss << "F2 - Create Vehicle (with driver)" << std::endl;
		ss << "F3 - Create Vehicle (with player)" << std::endl;
		ss << "F4 - Create Vehicle (empty)" << std::endl;
		ss << "F6 - Create all Vehicles" << std::endl;
		ss << "F7 - Create all Pedestrians" << std::endl;
		ss << "F9 - Display Object Information" << std::endl;
	}
	
	sf::Text text(ss.str(), font, 15);
	text.setPosition(10, 10);
	window.draw(text);
	
	while( gta->log.size() > 0 && gta->log.front().time + 10.f < gta->gameTime ) {
		gta->log.pop_front();
	}
	
	sf::Vector2f tpos(10.f, window.getSize().y - 30.f);
	text.setCharacterSize(15);
	for(auto it = gta->log.begin(); it != gta->log.end(); ++it) {
		text.setString(it->message);
		switch(it->type) {
		case GameWorld::LogEntry::Error:
			text.setColor(sf::Color::Red);
			break;
		case GameWorld::LogEntry::Warning:
			text.setColor(sf::Color::Yellow);
			break;
		default:
			text.setColor(sf::Color::White);
			break;
		}
		
		// Interpolate the color
		auto c = text.getColor();
		c.a = (gta->gameTime - it->time > 5.f) ? 255 - (((gta->gameTime - it->time) - 5.f)/5.f) * 255 : 255;
		text.setColor(c);
		
		text.setPosition(tpos);
		window.draw(text);
		tpos.y -= text.getLocalBounds().height;
	}
	
	static size_t fc = 0;
	if(fc++ == 60) 
	{
		std::cout << "Rendered: " << gta->renderer.rendered << " / Culled: " << gta->renderer.culled << std::endl;
		fc = 0;
	}
}

GenericState pauseState(
		[](State* self)
		{
			Menu *m = new Menu(font);
			m->offset = glm::vec2(50.f, 100.f);
			m->addEntry(Menu::lambda("Continue", [] { StateManager::get().exit(); }));
			m->addEntry(Menu::lambda("Options", [] { std::cout << "Options" << std::endl; }));
			m->addEntry(Menu::lambda("Exit", [] { window.close(); }));
			self->enterMenu(m);
			lockCursor(false);
		},
		[](State* self, float dt)
		{
			
		},
		[](State* self)
		{
			delete self->currentMenu;
		},
		[](State* self, const sf::Event& e)
		{
			switch(e.type) {
				case sf::Event::KeyPressed:
					switch(e.key.code) {
						case sf::Keyboard::Escape:
							StateManager::get().exit();
							break;
						default: break;
					}
					break;
				default: break;
			}
		}
);

GenericState gameState(
	[](State* self)
	{
		lockCursor(true);
		// TODO: create game state object
		// so we can track if we already
		// Started or not.
		if(! player) {
			command("player");
		}
	},
	[](State* self, float dt)
	{
		
	},
	[](State* self)
	{
		
	},
	[](State* self, const sf::Event& e)
	{
		switch(e.type) {
			case sf::Event::KeyPressed:
				switch(e.key.code) {
					case sf::Keyboard::Escape:
						StateManager::get().enter(&pauseState);
						break;
					default: break;
				}
				break;
			default: break;
		}
	}
);
	
GenericState menuState(
		[](State* self)
		{
			Menu *m = new Menu(font);
			m->offset = glm::vec2(50.f, 100.f);
			m->addEntry(Menu::lambda("Test", [] { StateManager::get().enter(&gameState); }));
			m->addEntry(Menu::lambda("Options", [] { std::cout << "Options" << std::endl; }));
			m->addEntry(Menu::lambda("Exit", [] { window.close(); }));
			self->enterMenu(m);
			lockCursor(false);
		},
		[](State* self, float dt)
		{
			
		},
		[](State* self)
		{
		},
		[](State* self, const sf::Event& e)
		{
			switch(e.type) {
				case sf::Event::KeyPressed:
					switch(e.key.code) {
						case sf::Keyboard::Escape:
							StateManager::get().exit();
						default: break;
					}
					break;
				default: break;
			}
		}
);

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " <path to GTA3 root folder>" << std::endl;
		exit(1);
	}
	
	if(! font.loadFromFile("DejaVuSansMono.ttf")) {
		std::cerr << "Failed to load font" << std::endl;
	}

	glewExperimental = GL_TRUE;
	glewInit();

	bool loadWorld = false;
	size_t w = WIDTH, h = HEIGHT;
	int c;
	while( (c = getopt(argc, argv, "w:h:l")) != -1) {
		switch(c) {
			case 'w':
				w = atoi(optarg);
				break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'l':
				loadWorld = true;
				break;
		}
	}

    sf::ContextSettings cs;
    cs.depthBits = 32;
    window.create(sf::VideoMode(w, h), "", sf::Style::Default, cs);
	window.setVerticalSyncEnabled(true);
	window.setMouseCursorVisible(false);

	init(argv[optind], loadWorld);
	
	sf::Clock clock;
	
	StateManager::get().enter(&menuState);
	
	float accum = 0.f;
    float ts = 1.f / 60.f;
	
	// Loop until the window is closed or we run out of state.
	while (window.isOpen() && StateManager::get().states.size()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			handleGlobalEvent(event);
			handleCommandEvent(event);
			handleInputEvent(event);
			
			StateManager::get().states.back()->handleEvent(event);
		}

		accum += clock.restart().asSeconds();
		
		while ( accum >= ts ) {
			update(ts);
			accum -= ts;
		}
		
		render();
		
		StateManager::get().draw(window);
		
		window.display();
	
	}

	return 0;
}
